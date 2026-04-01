#include "config.h"
#include "simulation_state.h"
#include <Arduino.h>
#include <Wire.h>
#include <cstring> // memcmp para dirty-check
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Bounce2.h>
#include <ESP32Encoder.h>
#include <freertos/semphr.h>

// ════════════════════════════════════════════════════════════
//  UI — Display OLED SH1107 128x128 + Botões + Encoder Rotativo
// ════════════════════════════════════════════════════════════

#define OLED_W        128
#define OLED_H        128
#define PARAM_COUNT    16   // total de parâmetros navegáveis
#define VISIBLE_ROWS    8   // linhas visíveis por página
#define ROW_SPACING    13   // px entre linhas de parâmetro
// Layout vertical:
//   y=0   → Header  (protocolo ativo)
//   y=9   → Linha separadora
//   y=11  → Param 0 da página  (+ i * ROW_SPACING)
//   y=119 → Footer  (hint contextual)

static Adafruit_SH1107  oled(OLED_W, OLED_H, &Wire);
static ESP32Encoder      encoder;
static Bounce            btn_prev, btn_next, btn_up, btn_down, btn_sel, btn_proto, btn_enc;

static SimulationState*  s_state  = nullptr;
static SemaphoreHandle_t s_mutex  = nullptr;

static uint8_t  s_cursor   = 0;      // parâmetro selecionado (0–15)
static bool     s_editing  = false;
static int32_t  s_enc_last = 0;

// ── Nomes curtos (máx. 9 chars) para caber no display ────────

static const char* PARAM_NAMES[PARAM_COUNT] = {
    "RPM",      "Vel km/h", "T.Motor C", "T.Admis C",
    "MAF g/s",  "MAP kPa",  "TPS %",    "Avanco gr",
    "Carga %",  "Comb. %",  "Bat. V",   "Oleo C",
    "STFT %",   "LTFT %",   "DTC",      "VIN"
};

// ── Leitura do valor atual de cada parâmetro ─────────────────

static String getParamValue(const SimulationState& s, uint8_t idx) {
    switch (idx) {
        case 0:  return String(s.rpm);
        case 1:  return String(s.speed_kmh);
        case 2:  return String(s.coolant_temp_c);
        case 3:  return String(s.intake_temp_c);
        case 4:  return String(s.maf_gs, 1);
        case 5:  return String(s.map_kpa);
        case 6:  return String(s.throttle_pct);
        case 7:  return String(s.ignition_adv, 1);
        case 8:  return String(s.engine_load_pct);
        case 9:  return String(s.fuel_level_pct);
        case 10: return String(s.battery_voltage, 1);
        case 11: return String(s.oil_temp_c);
        case 12: return String(s.stft_pct, 1);
        case 13: return String(s.ltft_pct, 1);
        case 14: return String(s.dtc_count) + " DTC";
        case 15: return String(s.vin);
        default: return "";
    }
}

// ── Incrementa/decrementa parâmetro via botão ou encoder ─────

static void adjustParam(SimulationState& s, uint8_t idx, int delta) {
    switch (idx) {
        case 0:  s.rpm             = constrain((int)s.rpm + delta * 50, 0, 16000);       break;
        case 1:  s.speed_kmh       = constrain((int)s.speed_kmh + delta * 5, 0, 255);   break;
        case 2:  s.coolant_temp_c  = constrain((int)s.coolant_temp_c + delta * 5, -40, 215); break;
        case 3:  s.intake_temp_c   = constrain((int)s.intake_temp_c + delta, -40, 80);  break;
        case 4:  s.maf_gs          = constrain(s.maf_gs + delta * 0.5f, 0.0f, 655.0f); break;
        case 5:  s.map_kpa         = constrain((int)s.map_kpa + delta * 5, 0, 255);     break;
        case 6:  s.throttle_pct    = constrain((int)s.throttle_pct + delta * 5, 0, 100); break;
        case 7:  s.ignition_adv    = constrain(s.ignition_adv + delta * 1.0f, -64.0f, 63.5f); break;
        case 8:  s.engine_load_pct = constrain((int)s.engine_load_pct + delta * 5, 0, 100); break;
        case 9:  s.fuel_level_pct  = constrain((int)s.fuel_level_pct + delta * 5, 0, 100); break;
        case 10: s.battery_voltage = constrain(s.battery_voltage + delta * 0.1f, 10.0f, 16.0f); break;
        case 11: s.oil_temp_c      = constrain((int)s.oil_temp_c + delta * 5, -40, 210); break;
        case 12: s.stft_pct        = constrain(s.stft_pct + delta * 0.8f, -30.0f, 30.0f); break;
        case 13: s.ltft_pct        = constrain(s.ltft_pct + delta * 0.8f, -30.0f, 30.0f); break;
        default: break; // DTC (14) e VIN (15) não são editáveis aqui
    }
}

// ── Render OLED 128x128 ───────────────────────────────────────

static void render(const SimulationState& s) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SH110X_WHITE);

    // ── Header: protocolo ativo (y=0) ─────────────────────────
    oled.setCursor(0, 0);
    oled.printf("%-20s", protoName(s.active_protocol));

    // ── Linha separadora (y=9) ────────────────────────────────
    oled.drawFastHLine(0, 9, OLED_W, SH110X_WHITE);

    // ── Parâmetros visíveis — página de VISIBLE_ROWS itens ────
    // Scroll por blocos: cursor 0-7 → página 0, 8-15 → página 1
    uint8_t page_start = (s_cursor / VISIBLE_ROWS) * VISIBLE_ROWS;
    for (uint8_t i = 0; i < VISIBLE_ROWS && (page_start + i) < PARAM_COUNT; i++) {
        uint8_t idx = page_start + i;
        int16_t y   = 11 + i * ROW_SPACING;
        oled.setCursor(0, y);

        if (idx == s_cursor) {
            if (s_editing) {
                oled.setTextColor(SH110X_BLACK, SH110X_WHITE); // destaque invertido
            } else {
                oled.print("> ");
            }
        } else {
            oled.print("  ");
        }

        oled.printf("%-9.9s %s", PARAM_NAMES[idx], getParamValue(s, idx).c_str());
        oled.setTextColor(SH110X_WHITE);
    }

    // ── Footer: hint contextual + indicador de página (y=119) ─
    oled.setCursor(0, 119);
    if (s_editing) {
        oled.print("UP/DN=ajust OK=sair"); // 19c
    } else {
        uint8_t page  = s_cursor / VISIBLE_ROWS + 1;
        uint8_t pages = (PARAM_COUNT + VISIBLE_ROWS - 1) / VISIBLE_ROWS;
        oled.printf("OK=edit  p.%d/%d", page, pages); // 14c — cabe folgado
    }

    oled.display();
}

// ── Task FreeRTOS ─────────────────────────────────────────────

static void task_ui(void*) {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(400000UL); // I2C Fast Mode 400kHz — reduz tempo de frame:
                             // 128×128 = 2048 bytes → ~41ms (vs ~164ms a 100kHz)

    if (!oled.begin(OLED_I2C_ADDR, false)) {
        // false = sem pino de reset (módulo 4-fios: VCC GND SCL SDA)
        Serial.println("[OLED] SH1107 não encontrado em 0x3C!");
    }
    // Ajuste de rotação: 0=normal, 1=90°, 2=180°, 3=270°
    // Mude para setRotation(1) se a imagem aparecer rotacionada
    oled.setRotation(0);
    oled.setContrast(0xFF);
    oled.clearDisplay();
    oled.display();

    btn_prev.attach(PIN_BTN_PREV,     INPUT_PULLUP); btn_prev.interval(BTN_DEBOUNCE_MS);
    btn_next.attach(PIN_BTN_NEXT,     INPUT_PULLUP); btn_next.interval(BTN_DEBOUNCE_MS);
    btn_up.attach  (PIN_BTN_UP,       INPUT_PULLUP); btn_up.interval(BTN_DEBOUNCE_MS);
    btn_down.attach(PIN_BTN_DOWN,     INPUT_PULLUP); btn_down.interval(BTN_DEBOUNCE_MS);
    btn_sel.attach (PIN_BTN_SELECT,   INPUT_PULLUP); btn_sel.interval(BTN_DEBOUNCE_MS);
    btn_proto.attach(PIN_BTN_PROTOCOL,INPUT_PULLUP); btn_proto.interval(BTN_DEBOUNCE_MS);
    btn_enc.attach (PIN_ENC_SW,       INPUT_PULLUP); btn_enc.interval(BTN_DEBOUNCE_MS);

    encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
    encoder.setCount(0);

    while (true) {
        btn_prev.update(); btn_next.update();
        btn_up.update();   btn_down.update();
        btn_sel.update();  btn_proto.update();
        btn_enc.update();

        xSemaphoreTake(s_mutex, portMAX_DELAY);

        if (!s_editing) {
            if (btn_prev.fell()) s_cursor = (s_cursor + PARAM_COUNT - 1) % PARAM_COUNT;
            if (btn_next.fell()) s_cursor = (s_cursor + 1)                % PARAM_COUNT;
            if (btn_sel.fell() || btn_enc.fell()) s_editing = true;
            if (btn_proto.fell()) {
                s_state->active_protocol = (s_state->active_protocol + 1) % PROTO_COUNT;
            }
        } else {
            if (btn_up.fell()   || btn_up.read()   == LOW) adjustParam(*s_state, s_cursor, +1);
            if (btn_down.fell() || btn_down.read() == LOW) adjustParam(*s_state, s_cursor, -1);
            if (btn_sel.fell() || btn_enc.fell()) s_editing = false;
        }

        // Encoder rotativo
        int32_t enc_now   = encoder.getCount();
        int32_t enc_delta = enc_now - s_enc_last;
        if (enc_delta != 0) {
            if (s_editing) adjustParam(*s_state, s_cursor, enc_delta > 0 ? 1 : -1);
            else           s_cursor = (s_cursor + (enc_delta > 0 ? 1 : PARAM_COUNT - 1)) % PARAM_COUNT;
            s_enc_last = enc_now;
        }

        SimulationState snap        = *s_state;
        uint8_t         snap_cursor = s_cursor;
        bool            snap_edit   = s_editing;
        xSemaphoreGive(s_mutex);

        // ── Só redesenha se algo mudou (evita flickering) ─────
        static SimulationState prev_snap   = {};
        static uint8_t         prev_cursor = 0xFF; // inválido → força 1º render
        static bool            prev_edit   = false;

        bool changed = (memcmp(&snap, &prev_snap, sizeof(SimulationState)) != 0)
                    || (snap_cursor != prev_cursor)
                    || (snap_edit   != prev_edit);

        if (changed) {
            render(snap);
            prev_snap   = snap;
            prev_cursor = snap_cursor;
            prev_edit   = snap_edit;
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // poll botões a 20Hz; render só se mudou
    }
}

// ── API pública ───────────────────────────────────────────────

void ui_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(task_ui, "task_ui", 8192, nullptr, 3, nullptr, 1);
}
