#include "config.h"
#include "simulation_state.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>
#include <ESP32Encoder.h>
#include <freertos/semphr.h>

// ════════════════════════════════════════════════════════════
//  UI — Display OLED + Botões + Encoder Rotativo
// ════════════════════════════════════════════════════════════

#define OLED_W 128
#define OLED_H 64

static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static ESP32Encoder      encoder;
static Bounce            btn_prev, btn_next, btn_up, btn_down, btn_sel, btn_proto, btn_enc;

static SimulationState*  s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

static uint8_t  s_cursor   = 0;  // parâmetro selecionado (0–11)
static bool     s_editing  = false;
static int32_t  s_enc_last = 0;

// ── Nomes dos parâmetros para o OLED ─────────────────────────

static const char* PARAM_NAMES[] = {
    "RPM", "Vel km/h", "Temp.Ref C", "Temp.Ar C",
    "MAF g/s", "MAP kPa", "TPS %", "Avanco grau",
    "Carga %", "Comb. %", "DTC", "VIN"
};

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
        case 10: return String(s.dtc_count) + " DTC(s)";
        case 11: return String(s.vin);
        default: return "";
    }
}

// ── Incrementa/decrementa parâmetro ──────────────────────────

static void adjustParam(SimulationState& s, uint8_t idx, int delta) {
    switch (idx) {
        case 0: s.rpm = constrain((int)s.rpm + delta * 50, 0, 16000); break;
        case 1: s.speed_kmh = constrain((int)s.speed_kmh + delta * 5, 0, 255); break;
        case 2: s.coolant_temp_c = constrain((int)s.coolant_temp_c + delta * 5, -40, 215); break;
        case 3: s.intake_temp_c = constrain((int)s.intake_temp_c + delta, -40, 80); break;
        case 4: s.maf_gs = constrain(s.maf_gs + delta * 0.5f, 0.0f, 655.0f); break;
        case 5: s.map_kpa = constrain((int)s.map_kpa + delta * 5, 0, 255); break;
        case 6: s.throttle_pct = constrain((int)s.throttle_pct + delta * 5, 0, 100); break;
        case 7: s.ignition_adv = constrain(s.ignition_adv + delta * 1.0f, -64.0f, 63.5f); break;
        case 8: s.engine_load_pct = constrain((int)s.engine_load_pct + delta * 5, 0, 100); break;
        case 9: s.fuel_level_pct = constrain((int)s.fuel_level_pct + delta * 5, 0, 100); break;
        default: break;
    }
}

// ── Render OLED ───────────────────────────────────────────────

static void render(const SimulationState& s) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    // Header: protocolo ativo
    oled.setCursor(0, 0);
    oled.printf("Proto: %s", protoName(s.active_protocol));

    // Mostra 4 parâmetros (scroll de 3 em 3)
    uint8_t start = (s_cursor / 4) * 4;
    for (uint8_t i = 0; i < 4 && (start + i) < 12; i++) {
        uint8_t idx = start + i;
        oled.setCursor(0, 10 + i * 13);
        if (idx == s_cursor) {
            if (s_editing) {
                oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // invertido
            } else {
                oled.print("> ");
            }
        } else {
            oled.print("  ");
        }
        oled.printf("%-9s %s", PARAM_NAMES[idx], getParamValue(s, idx).c_str());
        oled.setTextColor(SSD1306_WHITE);
    }

    // Footer
    oled.setCursor(0, 56);
    oled.print(s_editing ? "UP/DN=ajust OK=sair" : "OK=edit PROTO=troca");

    oled.display();
}

// ── Task FreeRTOS ─────────────────────────────────────────────

static void task_ui(void*) {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        Serial.println("[OLED] Não encontrado!");
    }
    oled.clearDisplay();

    btn_prev.attach(PIN_BTN_PREV, INPUT_PULLUP);   btn_prev.interval(BTN_DEBOUNCE_MS);
    btn_next.attach(PIN_BTN_NEXT, INPUT_PULLUP);   btn_next.interval(BTN_DEBOUNCE_MS);
    btn_up.attach(PIN_BTN_UP, INPUT_PULLUP);       btn_up.interval(BTN_DEBOUNCE_MS);
    btn_down.attach(PIN_BTN_DOWN, INPUT_PULLUP);   btn_down.interval(BTN_DEBOUNCE_MS);
    btn_sel.attach(PIN_BTN_SELECT, INPUT_PULLUP);  btn_sel.interval(BTN_DEBOUNCE_MS);
    btn_proto.attach(PIN_BTN_PROTOCOL, INPUT_PULLUP); btn_proto.interval(BTN_DEBOUNCE_MS);
    btn_enc.attach(PIN_ENC_SW, INPUT_PULLUP);      btn_enc.interval(BTN_DEBOUNCE_MS);

    encoder.attachHalfQuad(PIN_ENC_A, PIN_ENC_B);
    encoder.setCount(0);

    while (true) {
        btn_prev.update(); btn_next.update();
        btn_up.update();   btn_down.update();
        btn_sel.update();  btn_proto.update();
        btn_enc.update();

        xSemaphoreTake(s_mutex, portMAX_DELAY);

        if (!s_editing) {
            if (btn_prev.fell()) s_cursor = (s_cursor + 11) % 12;
            if (btn_next.fell()) s_cursor = (s_cursor + 1)  % 12;
            if (btn_sel.fell() || btn_enc.fell()) s_editing = true;
            if (btn_proto.fell()) {
                s_state->active_protocol = (s_state->active_protocol + 1) % PROTO_COUNT;
            }
        } else {
            if (btn_up.fell()   || btn_up.read()   == LOW) adjustParam(*s_state, s_cursor, +1);
            if (btn_down.fell() || btn_down.read()  == LOW) adjustParam(*s_state, s_cursor, -1);
            if (btn_sel.fell() || btn_enc.fell()) s_editing = false;
        }

        // Encoder
        int32_t enc_now = encoder.getCount();
        int32_t enc_delta = enc_now - s_enc_last;
        if (enc_delta != 0) {
            if (s_editing) adjustParam(*s_state, s_cursor, enc_delta > 0 ? 1 : -1);
            else s_cursor = (s_cursor + (enc_delta > 0 ? 1 : 11)) % 12;
            s_enc_last = enc_now;
        }

        SimulationState snap = *s_state;
        xSemaphoreGive(s_mutex);

        render(snap);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── API pública ───────────────────────────────────────────────

void ui_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(task_ui, "task_ui", 8192, nullptr, 3, nullptr, 1);
}
