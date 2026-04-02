#include "elm327_bt.h"
#include "config.h"
#include "obd_types.h"
#include "obd_dispatcher.h"
#include <Arduino.h>
#include <BluetoothSerial.h>

// ════════════════════════════════════════════════════════════
//  Bluetooth SPP — Emulador ELM327
//
//  Permite que apps como Torque Pro, OBD Auto Doctor e Car
//  Scanner se conectem ao simulador via Bluetooth Classic.
//  Funciona em paralelo com o protocolo físico (CAN/K-Line).
//
//  Protocolo: texto ASCII, comandos AT + hex OBD-II
//  Formato de resposta: compatível com ELM327 v1.5
// ════════════════════════════════════════════════════════════

static BluetoothSerial  SerialBT;
static SimulationState* s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static OBDDispatcher     s_dispatcher;

// ── Estado do ELM327 ─────────────────────────────────────────

static struct ElmState {
    bool echo       = true;   // AT E0/E1
    bool headers    = false;  // AT H0/H1
    bool spaces     = true;   // AT S0/S1
    bool linefeeds  = true;   // AT L0/L1

    void reset() {
        echo      = true;
        headers   = false;
        spaces    = true;
        linefeeds = true;
    }
} s_elm;

// ── Helpers ──────────────────────────────────────────────────

static void bt_send(const char* s) {
    SerialBT.print(s);
}

static void bt_reply(const String& msg) {
    // Formato ELM327: resposta + \r + prompt ">"
    SerialBT.print(msg);
    SerialBT.print("\r");
    if (s_elm.linefeeds) SerialBT.print("\n");
}

static void bt_prompt() {
    SerialBT.print("\r>");
}

static void bt_ok() {
    bt_reply("OK");
    bt_prompt();
}

// Converte nibble hex → valor (0-15), retorna -1 se inválido
static int8_t hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

// Formata byte como 2 chars hex maiúsculos
static const char HEX_CHARS[] = "0123456789ABCDEF";
static void hex_byte(char* buf, uint8_t val) {
    buf[0] = HEX_CHARS[(val >> 4) & 0x0F];
    buf[1] = HEX_CHARS[val & 0x0F];
}

// ── Processamento de comandos AT ─────────────────────────────

static void handle_at_command(const String& cmd) {
    // cmd já está uppercase e sem espaços, ex: "ATZ", "ATE0", "ATSP6"
    String at = cmd.substring(2);  // remove "AT"

    if (at == "Z") {
        // Reset
        s_elm.reset();
        bt_reply("");
        bt_reply(ELM_VERSION);
        bt_prompt();
    }
    else if (at == "I") {
        bt_reply(ELM_VERSION);
        bt_prompt();
    }
    else if (at == "@1") {
        bt_reply(ELM_DEVICE_DESC);
        bt_prompt();
    }
    else if (at == "E0") { s_elm.echo = false; bt_ok(); }
    else if (at == "E1") { s_elm.echo = true;  bt_ok(); }
    else if (at == "H0") { s_elm.headers = false; bt_ok(); }
    else if (at == "H1") { s_elm.headers = true;  bt_ok(); }
    else if (at == "L0") { s_elm.linefeeds = false; bt_ok(); }
    else if (at == "L1") { s_elm.linefeeds = true;  bt_ok(); }
    else if (at == "S0") { s_elm.spaces = false; bt_ok(); }
    else if (at == "S1") { s_elm.spaces = true;  bt_ok(); }
    else if (at == "D0" || at == "D1")   { bt_ok(); }  // DLC display — aceita, ignora
    else if (at.startsWith("SP"))        { bt_ok(); }  // Set Protocol — aceita qualquer
    else if (at == "DPN")                { bt_reply("6"); bt_prompt(); } // CAN 11-bit 500k
    else if (at == "DP")                 { bt_reply("ISO 15765-4 (CAN 11/500)"); bt_prompt(); }
    else if (at.startsWith("ST"))        { bt_ok(); }  // Set Timeout — ignora
    else if (at.startsWith("AT"))        { bt_ok(); }  // Adaptive Timing — ignora
    else if (at == "BI")                 { bt_ok(); }  // Bypass Init
    else if (at == "CAF0" || at == "CAF1") { bt_ok(); } // CAN Auto Formatting
    else if (at == "AL")                 { bt_ok(); }  // Allow Long messages
    else if (at == "MA")                 { bt_ok(); }  // Monitor All
    else if (at == "PC")                 { bt_ok(); }  // Protocol Close
    else if (at == "WS")                 { // Warm Start (= reset sem banner)
        s_elm.reset();
        bt_prompt();
    }
    else if (at == "RV") {
        // Read Voltage — retorna tensão da bateria do simulador
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        float v = s_state->battery_voltage;
        xSemaphoreGive(s_mutex);
        bt_reply(String(v, 1) + "V");
        bt_prompt();
    }
    else {
        // Comando desconhecido — responde OK (não queremos "?" que trava apps)
        bt_ok();
    }
}

// ── Processamento de requisição OBD hex ──────────────────────

static void handle_obd_request(const String& hex) {
    // hex é string de hex puro, ex: "010C", "03", "0902"
    // Converte para bytes
    uint8_t raw[8];
    uint8_t raw_len = 0;
    for (size_t i = 0; i + 1 < hex.length() && raw_len < 8; i += 2) {
        int8_t hi = hex_val(hex[i]);
        int8_t lo = hex_val(hex[i + 1]);
        if (hi < 0 || lo < 0) { bt_reply("?"); bt_prompt(); return; }
        raw[raw_len++] = (uint8_t)((hi << 4) | lo);
    }
    if (raw_len == 0) { bt_reply("?"); bt_prompt(); return; }

    // Monta OBDRequest
    OBDRequest req = {};
    req.mode = raw[0];
    if (raw_len > 1) req.pid = raw[1];
    for (uint8_t i = 2; i < raw_len; i++) {
        req.data[i - 2] = raw[i];
        req.data_len = i - 1;
    }

    // Dispatch com mutex
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    OBDResponse resp = s_dispatcher.dispatch(req, *s_state);
    // Mode 04 — clear DTCs (mesmo tratamento do CAN protocol)
    if (req.mode == 0x04) {
        s_state->dtc_count = 0;
        memset(s_state->dtcs, 0, sizeof(s_state->dtcs));
    }
    xSemaphoreGive(s_mutex);

    // Verifica resposta negativa
    if (resp.negative) {
        bt_reply("NO DATA");
        bt_prompt();
        return;
    }

    // ── Formata resposta como hex string (estilo ELM327) ──────
    //
    // Dispatcher retorna: [ModeByte][dados...]
    // Para Mode 01/09: precisamos inserir PID após ModeByte
    // Para Mode 03/04: não tem PID
    //
    // Formato CAN single frame com header:
    //   7E8 NN MM PP DD DD DD DD
    //   NN = N_PCI (número de bytes de dados)
    //   MM = mode byte (0x41, 0x43, 0x44, 0x49)
    //   PP = PID (se aplicável)
    //   DD = dados

    // Monta payload: insere PID para Mode 01/09
    uint8_t payload[16];
    uint8_t plen = 0;
    bool has_pid = (req.mode == 0x01 || req.mode == 0x09);

    if (has_pid) {
        // [ModeByte][PID][dados do handler...]
        payload[plen++] = resp.data[0];        // mode byte (0x41, 0x49)
        payload[plen++] = req.pid;             // PID
        for (uint8_t i = 1; i < resp.len && plen < 16; i++)
            payload[plen++] = resp.data[i];    // dados
    } else {
        // Mode 03/04: [ModeByte][dados...]
        for (uint8_t i = 0; i < resp.len && plen < 16; i++)
            payload[plen++] = resp.data[i];
    }

    // Constrói string de saída
    String out;
    out.reserve(64);

    if (s_elm.headers) {
        // CAN header: "7E8 "
        out += "7E8";
        if (s_elm.spaces) out += ' ';
        // N_PCI (single frame)
        char npci[3] = {0};
        hex_byte(npci, plen);
        out += npci[0]; out += npci[1];
        if (s_elm.spaces) out += ' ';
    }

    // Dados do payload
    for (uint8_t i = 0; i < plen; i++) {
        char hb[3] = {0};
        hex_byte(hb, payload[i]);
        out += hb[0]; out += hb[1];
        if (s_elm.spaces && i < plen - 1) out += ' ';
    }

    bt_reply(out);
    bt_prompt();
}

// ── Processamento de linha recebida ──────────────────────────

static void process_line(const String& raw_line) {
    // Remove espaços e converte para maiúsculo
    String line;
    line.reserve(raw_line.length());
    for (size_t i = 0; i < raw_line.length(); i++) {
        char c = raw_line[i];
        if (c == ' ' || c == '\t' || c == '\n') continue;
        if (c >= 'a' && c <= 'z') c -= 32;  // uppercase
        line += c;
    }
    if (line.length() == 0) { bt_prompt(); return; }

    // Echo?
    if (s_elm.echo) {
        bt_reply(line);
    }

    // AT command ou OBD hex?
    if (line.startsWith("AT")) {
        handle_at_command(line);
    } else {
        handle_obd_request(line);
    }
}

// ── Task FreeRTOS ────────────────────────────────────────────

static void task_elm327_bt(void*) {
    String rx_buf;
    rx_buf.reserve(64);

    while (true) {
        if (!SerialBT.hasClient()) {
            // Sem cliente — espera
            vTaskDelay(pdMS_TO_TICKS(200));
            rx_buf = "";
            continue;
        }

        // Lê bytes disponíveis
        while (SerialBT.available()) {
            char c = (char)SerialBT.read();

            if (c == '\r') {
                // Fim de comando — processa
                if (rx_buf.length() > 0) {
                    process_line(rx_buf);
                    rx_buf = "";
                }
            } else if (c == '\n') {
                // Ignora LF
            } else if (rx_buf.length() < 60) {
                rx_buf += c;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  // 50 Hz — responsivo sem busy-wait
    }
}

// ── Init público ─────────────────────────────────────────────

void elm327_bt_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;

    SerialBT.begin(BT_DEVICE_NAME);
    Serial.printf("[BT] SPP iniciado: '%s' (PIN %s)\n", BT_DEVICE_NAME, BT_PIN);

    xTaskCreatePinnedToCore(
        task_elm327_bt, "elm327_bt",
        8192,       // stack — BT stack precisa mais memória
        nullptr,
        2,          // prioridade
        nullptr,
        1           // Core 1
    );
}

bool elm327_bt_connected() {
    return SerialBT.hasClient();
}
