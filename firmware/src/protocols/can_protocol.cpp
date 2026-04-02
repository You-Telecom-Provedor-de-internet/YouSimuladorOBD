#include "can_protocol.h"
#include "config.h"
#include <Arduino.h>
#include <driver/twai.h>

// ════════════════════════════════════════════════════════════
//  ISO 15765-4 — CAN Protocol via TWAI (nativo ESP32)
//  Suporta: 11-bit 500k, 11-bit 250k, 29-bit 500k, 29-bit 250k
// ════════════════════════════════════════════════════════════

static OBDDispatcher  s_dispatcher;
static SimulationState* s_state  = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static uint8_t          s_proto  = PROTO_CAN_11B_500K;

// ── Configuração TWAI ─────────────────────────────────────────

static bool twai_start(uint8_t protocol) {
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, TWAI_MODE_NORMAL);
    g.tx_queue_len = 10;
    g.rx_queue_len = 20;

    twai_timing_config_t t;
    if (protocol == PROTO_CAN_11B_500K || protocol == PROTO_CAN_29B_500K)
        t = TWAI_TIMING_CONFIG_500KBITS();
    else
        t = TWAI_TIMING_CONFIG_250KBITS();

    // Aceita 0x7DF (broadcast) e 0x7E0 (físico ECU1) para 11-bit
    // Para 29-bit aceita 0x18DB33F1 e 0x18DA10F1
    bool extended = (protocol == PROTO_CAN_29B_500K || protocol == PROTO_CAN_29B_250K);
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
    if (twai_start() != ESP_OK) { twai_driver_uninstall(); return false; }
    return true;
}

// ── Parsing do frame recebido ─────────────────────────────────

static ParseResult parse_frame(const twai_message_t& msg, OBDRequest& req) {
    if (msg.data_length_code < 2) return ParseResult::INVALID;

    uint8_t n_pci = msg.data[0];
    uint8_t type  = (n_pci >> 4) & 0x0F;

    if (type != 0) return ParseResult::UNSUPPORTED; // só Single Frame por ora

    uint8_t dlen = n_pci & 0x0F;
    if (dlen < 2 || dlen > 6) return ParseResult::INVALID;

    req.mode     = msg.data[1];
    req.pid      = (dlen >= 2) ? msg.data[2] : 0x00;
    req.data_len = (dlen > 2)  ? dlen - 2    : 0;
    for (uint8_t i = 0; i < req.data_len; i++)
        req.data[i] = msg.data[3 + i];

    return ParseResult::OK;
}

// ── Montar e enviar frame de resposta ─────────────────────────

static void send_response(const OBDResponse& resp, uint8_t pid, bool extended) {
    twai_message_t tx = {};
    tx.extd         = extended ? 1 : 0;
    tx.identifier   = extended ? OBD_CAN_RESP_29B : OBD_CAN_RESP_11B;
    tx.data_length_code = 8;

    if (resp.negative) {
        tx.data[0] = 0x03;
        tx.data[1] = 0x7F;
        tx.data[2] = resp.nrc;
        // restante = 0x00 (padding)
    } else {
        // [N_PCI][dados...] — Single Frame
        uint8_t total = resp.len + 1; // +1 para o byte PID que precede os dados
        // Formato: [N_PCI=0x0N][Mode+0x40][PID][data...]
        // resp.data[0] já contém Mode+0x40 (inserido pelo dispatcher)
        // Precisamos inserir o PID após o mode byte
        uint8_t payload[7];
        payload[0] = resp.data[0]; // Mode+0x40
        payload[1] = pid;          // PID
        for (uint8_t i = 1; i < resp.len && i < 7; i++)
            payload[i + 1] = resp.data[i];

        tx.data[0] = (resp.len + 1) & 0x0F; // N_PCI: Single Frame, tamanho
        for (uint8_t i = 0; i < 7 && i < (resp.len + 1); i++)
            tx.data[1 + i] = payload[i];
    }

    twai_transmit(&tx, pdMS_TO_TICKS(10));

    // Pisca LED de atividade
    digitalWrite(PIN_LED_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(50));
    digitalWrite(PIN_LED_TX, LOW);
}

// ── Task FreeRTOS ─────────────────────────────────────────────

static void task_can(void*) {
    bool extended = (s_proto == PROTO_CAN_29B_500K || s_proto == PROTO_CAN_29B_250K);

    if (!twai_start(s_proto)) {
        Serial.println("[CAN] Falha ao iniciar TWAI!");
        vTaskDelete(nullptr);
        return;
    }
    digitalWrite(PIN_LED_CAN, HIGH);
    Serial.printf("[CAN] TWAI iniciado — %s\n", protoName(s_proto));

    while (true) {
        twai_message_t rx;
        if (twai_receive(&rx, pdMS_TO_TICKS(100)) == ESP_OK) {
            OBDRequest req{};
            if (parse_frame(rx, req) == ParseResult::OK) {
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                OBDResponse resp = s_dispatcher.dispatch(req, *s_state);
                // Se mode04 (clear DTCs), aplica no state
                if (req.mode == 0x04) s_state->dtc_count = 0;
                xSemaphoreGive(s_mutex);
                send_response(resp, req.pid, extended);
            }
        }
    }
}

// ── API pública ───────────────────────────────────────────────

void can_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t protocol) {
    s_state = state;
    s_mutex = mutex;
    s_proto = protocol;
    xTaskCreatePinnedToCore(task_can, "task_can", 4096, nullptr, 10, nullptr, 0);
}
