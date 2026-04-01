#include "kline_protocol.h"
#include "config.h"
#include <driver/uart.h>

// ════════════════════════════════════════════════════════════
//  ISO 9141-2 / ISO 14230-4 KWP2000 — K-Line via UART1
//  Pino TX: GPIO17,  RX: GPIO16
//  Hardware externo: L9637D (recomendado) ou circuito NPN discreto
//  A K-Line opera em 12V — o driver faz a conversão de nível.
// ════════════════════════════════════════════════════════════

static OBDDispatcher   s_dispatcher;
static SimulationState* s_state  = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static uint8_t           s_proto = PROTO_ISO9141;

// ── Inicializa UART para K-Line ───────────────────────────────

static void uart_kline_begin() {
    uart_config_t cfg = {
        .baud_rate  = (int)KLINE_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_port_t port = (uart_port_t)KLINE_UART_NUM;
    uart_driver_install(port, 256, 256, 0, nullptr, 0);
    uart_param_config(port, &cfg);
    uart_set_pin(port, PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ── Envia 1 byte a 5 baud bit-a-bit via GPIO (init ISO 9141-2) ─

static void send_5baud_byte(uint8_t byte) {
    // Configura TX como GPIO para controle manual do nível
    uart_set_pin((uart_port_t)KLINE_UART_NUM, UART_PIN_NO_CHANGE, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    pinMode(PIN_KLINE_TX, OUTPUT);

    // Start bit (LOW)
    digitalWrite(PIN_KLINE_TX, LOW);
    vTaskDelay(pdMS_TO_TICKS(200)); // 1/5 baud = 200ms

    // 8 bits LSB first
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_KLINE_TX, (byte >> i) & 1 ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // Stop bit (HIGH)
    digitalWrite(PIN_KLINE_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));

    // Restaura TX para UART
    uart_set_pin((uart_port_t)KLINE_UART_NUM, PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ── Fast Init (KWP2000 Fast) ──────────────────────────────────

static void fast_init_pulse() {
    pinMode(PIN_KLINE_TX, OUTPUT);
    digitalWrite(PIN_KLINE_TX, LOW);
    vTaskDelay(pdMS_TO_TICKS(25));
    digitalWrite(PIN_KLINE_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(25));
    uart_set_pin((uart_port_t)KLINE_UART_NUM, PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// ── Calcula checksum (soma dos bytes & 0xFF) ──────────────────

static uint8_t checksum(const uint8_t* buf, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += buf[i];
    return (uint8_t)(sum & 0xFF);
}

// ── Envia resposta K-Line (ISO 9141-2 / KWP2000) ─────────────

static void send_kline_response(const OBDResponse& resp, uint8_t pid, uint8_t mode_resp) {
    // Formato ISO 9141-2: [48][6B][F1][data...][CS]
    uint8_t buf[20];
    uint8_t len = 0;
    buf[len++] = 0x48; // header
    buf[len++] = 0x6B; // target (resposta ECU)
    buf[len++] = 0xF1; // source (testador)
    buf[len++] = mode_resp;
    buf[len++] = pid;
    for (uint8_t i = 1; i < resp.len; i++) // pula o mode byte (já em mode_resp)
        buf[len++] = resp.data[i];
    buf[len] = checksum(buf, len);
    len++;

    uart_write_bytes((uart_port_t)KLINE_UART_NUM, (const char*)buf, len);

    digitalWrite(PIN_LED_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(50));
    digitalWrite(PIN_LED_TX, LOW);
}

// ── Parser de frame K-Line recebido ──────────────────────────

static ParseResult parse_kline_frame(const uint8_t* buf, uint8_t len, OBDRequest& req) {
    // Mínimo: header(3) + mode(1) + pid(1) + cs(1) = 6 bytes
    if (len < 5) return ParseResult::INCOMPLETE;
    // buf[0]=0x68 ou 0x80, buf[1]=target, buf[2]=source
    req.mode     = buf[3];
    req.pid      = (len > 4) ? buf[4] : 0x00;
    req.data_len = 0;
    return ParseResult::OK;
}

// ── Task FreeRTOS ─────────────────────────────────────────────

static void task_kline(void*) {
    // Linha idle por 300ms
    vTaskDelay(pdMS_TO_TICKS(300));
    uart_kline_begin();

    // Sequência de init conforme protocolo
    if (s_proto == PROTO_ISO9141 || s_proto == PROTO_KWP_5BAUD) {
        send_5baud_byte(0x33); // endereço padrão OBD
        vTaskDelay(pdMS_TO_TICKS(20));
        // Responde sync 0x55 + key bytes
        uint8_t init_resp[] = {0x55, 0x08, 0x08};
        uart_write_bytes((uart_port_t)KLINE_UART_NUM, (const char*)init_resp, sizeof(init_resp));
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (s_proto == PROTO_KWP_FAST) {
        fast_init_pulse();
        vTaskDelay(pdMS_TO_TICKS(10));
        // StartCommunication Response
        uint8_t sc_resp[] = {0xC1, 0x57, 0x8F, 0xE9, 0x0A};
        uart_write_bytes((uart_port_t)KLINE_UART_NUM, (const char*)sc_resp, sizeof(sc_resp));
    }

    Serial.printf("[K-Line] Init OK — %s\n", protoName(s_proto));

    uint8_t rx_buf[16];
    uint8_t rx_len = 0;

    while (true) {
        uint8_t byte;
        int r = uart_read_bytes((uart_port_t)KLINE_UART_NUM, &byte, 1, pdMS_TO_TICKS(50));
        if (r > 0) {
            if (rx_len < sizeof(rx_buf)) rx_buf[rx_len++] = byte;

            OBDRequest req{};
            if (parse_kline_frame(rx_buf, rx_len, req) == ParseResult::OK) {
                rx_len = 0;
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                OBDResponse resp = s_dispatcher.dispatch(req, *s_state);
                if (req.mode == 0x04) s_state->dtc_count = 0;
                xSemaphoreGive(s_mutex);
                uint8_t mode_resp = resp.negative ? 0x7F : (req.mode + 0x40);
                send_kline_response(resp, req.pid, mode_resp);
            }
        } else {
            rx_len = 0; // timeout — reseta buffer
        }
    }
}

// ── API pública ───────────────────────────────────────────────

void kline_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t protocol) {
    s_state = state;
    s_mutex = mutex;
    s_proto = protocol;
    xTaskCreatePinnedToCore(task_kline, "task_kline", 4096, nullptr, 10, nullptr, 0);
}
