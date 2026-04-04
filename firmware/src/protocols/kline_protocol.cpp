#include "kline_protocol.h"
#include "config.h"
#include "diagnostic_scenario_engine.h"
#include <driver/uart.h>
#include <cstring>

static OBDDispatcher s_dispatcher;
static SimulationState* s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

static bool is_kline_protocol(uint8_t protocol) {
    return protocol >= PROTO_ISO9141 && protocol < PROTO_COUNT;
}

static uart_port_t kline_port() {
    return static_cast<uart_port_t>(KLINE_UART_NUM);
}

static bool uart_kline_begin() {
    uart_config_t cfg = {
        .baud_rate = static_cast<int>(KLINE_BAUD),
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    if (uart_driver_install(kline_port(), 256, 256, 0, nullptr, 0) != ESP_OK) {
        return false;
    }
    if (uart_param_config(kline_port(), &cfg) != ESP_OK) {
        uart_driver_delete(kline_port());
        return false;
    }
    if (uart_set_pin(kline_port(), PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        uart_driver_delete(kline_port());
        return false;
    }
    return true;
}

static void uart_kline_end() {
    uart_flush_input(kline_port());
    uart_driver_delete(kline_port());
}

static void send_5baud_byte(uint8_t byte) {
    uart_set_pin(kline_port(), UART_PIN_NO_CHANGE, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    pinMode(PIN_KLINE_TX, OUTPUT);

    digitalWrite(PIN_KLINE_TX, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));

    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_KLINE_TX, (byte >> i) & 1 ? HIGH : LOW);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    digitalWrite(PIN_KLINE_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));

    uart_set_pin(kline_port(), PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static void fast_init_pulse() {
    pinMode(PIN_KLINE_TX, OUTPUT);
    digitalWrite(PIN_KLINE_TX, LOW);
    vTaskDelay(pdMS_TO_TICKS(25));
    digitalWrite(PIN_KLINE_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(25));
    uart_set_pin(kline_port(), PIN_KLINE_TX, PIN_KLINE_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

static uint8_t checksum(const uint8_t* buf, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

static uint8_t build_kline_payload(const OBDResponse& resp, const OBDRequest& req, uint8_t* payload, size_t capacity) {
    uint8_t len = 0;

    if (resp.negative) {
        if (capacity < 3) {
            return 0;
        }
        payload[len++] = 0x7F;
        payload[len++] = req.mode;
        payload[len++] = resp.nrc;
        return len;
    }

    const bool has_pid = (req.mode == 0x01 || req.mode == 0x02 || req.mode == 0x09);
    if (has_pid) {
        if (capacity < 2 || resp.len == 0) {
            return 0;
        }
        payload[len++] = resp.data[0];
        payload[len++] = req.pid;
        for (uint8_t i = 1; i < resp.len && len < capacity; i++) {
            payload[len++] = resp.data[i];
        }
        return len;
    }

    for (uint8_t i = 0; i < resp.len && len < capacity; i++) {
        payload[len++] = resp.data[i];
    }
    return len;
}

static void send_kline_response(const OBDResponse& resp, const OBDRequest& req) {
    uint8_t payload[80] = {};
    const uint8_t payload_len = build_kline_payload(resp, req, payload, sizeof(payload));
    if (payload_len == 0) {
        return;
    }

    uint8_t frame[96] = {};
    uint8_t len = 0;
    frame[len++] = 0x48;
    frame[len++] = 0x6B;
    frame[len++] = 0xF1;
    memcpy(&frame[len], payload, payload_len);
    len += payload_len;
    frame[len++] = checksum(frame, len);

    uart_write_bytes(kline_port(), reinterpret_cast<const char*>(frame), len);

    digitalWrite(PIN_LED_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(30));
    digitalWrite(PIN_LED_TX, LOW);
}

static ParseResult parse_kline_frame(const uint8_t* buf, uint8_t len, OBDRequest& req) {
    if (len < 5) {
        return ParseResult::INCOMPLETE;
    }

    if ((buf[3] == 0x03 || buf[3] == 0x04) && checksum(buf, 4) == buf[4]) {
        req.mode = buf[3];
        req.pid = 0x00;
        req.data_len = 0;
        return ParseResult::OK;
    }

    if (len < 6) {
        return ParseResult::INCOMPLETE;
    }

    if (checksum(buf, len - 1) != buf[len - 1]) {
        return ParseResult::INCOMPLETE;
    }

    req.mode = buf[3];
    req.pid = buf[4];
    req.data_len = 0;

    if (len > 6) {
        req.data_len = len - 6;
        if (req.data_len > sizeof(req.data)) {
            return ParseResult::INVALID;
        }
        memcpy(req.data, &buf[5], req.data_len);
    }

    return ParseResult::OK;
}

static bool start_kline_protocol(uint8_t protocol) {
    if (!uart_kline_begin()) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(300));

    if (protocol == PROTO_ISO9141 || protocol == PROTO_KWP_5BAUD) {
        send_5baud_byte(0x33);
        vTaskDelay(pdMS_TO_TICKS(20));
        const uint8_t init_resp[] = {0x55, 0x08, 0x08};
        uart_write_bytes(kline_port(), reinterpret_cast<const char*>(init_resp), sizeof(init_resp));
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (protocol == PROTO_KWP_FAST) {
        fast_init_pulse();
        vTaskDelay(pdMS_TO_TICKS(10));
        const uint8_t sc_resp[] = {0xC1, 0x57, 0x8F, 0xE9, 0x0A};
        uart_write_bytes(kline_port(), reinterpret_cast<const char*>(sc_resp), sizeof(sc_resp));
    }

    Serial.printf("[K-Line] Init OK - %s\n", protoName(protocol));
    return true;
}

static void task_kline(void*) {
    bool started = false;
    uint8_t running_proto = 0xFF;
    uint8_t rx_buf[24] = {};
    uint8_t rx_len = 0;

    while (true) {
        uint8_t active_proto = 0;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        active_proto = s_state->active_protocol;
        xSemaphoreGive(s_mutex);

        if (!is_kline_protocol(active_proto)) {
            if (started) {
                uart_kline_end();
                started = false;
                running_proto = 0xFF;
                rx_len = 0;
                digitalWrite(PIN_LED_KLINE, LOW);
                Serial.println("[K-Line] UART parada");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!started || running_proto != active_proto) {
            if (started) {
                uart_kline_end();
                started = false;
                rx_len = 0;
            }
            if (!start_kline_protocol(active_proto)) {
                digitalWrite(PIN_LED_KLINE, LOW);
                Serial.printf("[K-Line] Falha ao iniciar UART para %s\n", protoName(active_proto));
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            started = true;
            running_proto = active_proto;
            digitalWrite(PIN_LED_KLINE, HIGH);
        }

        uint8_t byte = 0;
        int read = uart_read_bytes(kline_port(), &byte, 1, pdMS_TO_TICKS(50));
        if (read <= 0) {
            rx_len = 0;
            continue;
        }

        if (rx_len < sizeof(rx_buf)) {
            rx_buf[rx_len++] = byte;
        }

        OBDRequest req = {};
        ParseResult parse = parse_kline_frame(rx_buf, rx_len, req);
        if (parse == ParseResult::INCOMPLETE) {
            continue;
        }
        if (parse != ParseResult::OK) {
            rx_len = 0;
            continue;
        }

        rx_len = 0;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        OBDResponse resp = s_dispatcher.dispatch(req, *s_state);
        if (req.mode == 0x04) {
            diagnostic_engine_handle_mode04_clear(*s_state);
        }
        xSemaphoreGive(s_mutex);

        send_kline_response(resp, req);
    }
}

void kline_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(task_kline, "task_kline", 6144, nullptr, 10, nullptr, 0);
}
