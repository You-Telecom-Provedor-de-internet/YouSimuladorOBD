#include "kline_protocol.h"
#include "config.h"
#include "diagnostic_scenario_engine.h"
#include <Arduino.h>
#include <driver/uart.h>
#include <cstring>

static OBDDispatcher s_dispatcher;
static SimulationState* s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static bool s_started = false;
static uint8_t s_running_proto = 0xFF;
static uint32_t s_last_activity_ms = 0;
static constexpr uint32_t KLINE_SESSION_IDLE_TIMEOUT_MS = 3000;
static constexpr uint8_t KLINE_TESTER_ADDR = 0xF1;
static constexpr uint8_t KLINE_FUNC_ADDR = 0x33;
static constexpr uint8_t KLINE_ECU_ADDR = 0x11;

static bool is_kline_protocol(uint8_t protocol) {
    return protocol >= PROTO_ISO9141 && protocol < PROTO_COUNT;
}

static uart_port_t kline_port() {
    return static_cast<uart_port_t>(KLINE_UART_NUM);
}

static void prepare_kline_idle() {
    pinMode(PIN_KLINE_TX, OUTPUT);
    digitalWrite(PIN_KLINE_TX, HIGH);
    pinMode(PIN_KLINE_RX, INPUT);
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

static bool uart_kline_reconfigure(uart_word_length_t data_bits,
                                   uart_parity_t parity,
                                   uart_stop_bits_t stop_bits = UART_STOP_BITS_1) {
    uart_config_t cfg = {
        .baud_rate = static_cast<int>(KLINE_BAUD),
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    return uart_param_config(kline_port(), &cfg) == ESP_OK;
}

static void uart_kline_end() {
    uart_flush_input(kline_port());
    uart_driver_delete(kline_port());
}

static bool wait_for_5baud_address(uint8_t expected, uint32_t timeout_ms) {
    prepare_kline_idle();

    const uint32_t deadline = millis() + timeout_ms;
    int previous_level = digitalRead(PIN_KLINE_RX);
    uint32_t high_since_us = micros();
    uint32_t last_candidate_log_ms = 0;

    while (millis() < deadline) {
        const int current_level = digitalRead(PIN_KLINE_RX);
        if (previous_level == LOW && current_level == HIGH) {
            high_since_us = micros();
        }
        if (previous_level == HIGH && current_level == LOW) {
            const uint32_t high_duration_us = micros() - high_since_us;
            if (high_duration_us < 150000UL) {
                const uint32_t now_ms = millis();
                if ((now_ms - last_candidate_log_ms) > 250) {
                    Serial.printf("[K-Line] Borda ignorada: HIGH por %lu us\n",
                                  static_cast<unsigned long>(high_duration_us));
                    last_candidate_log_ms = now_ms;
                }
                previous_level = current_level;
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            const uint32_t start_us = micros();
            uint8_t value = 0;

            const uint32_t start_sample_us = start_us + 100000UL;
            while (static_cast<int32_t>(start_sample_us - micros()) > 0) {
                delayMicroseconds(50);
            }
            if (digitalRead(PIN_KLINE_RX) != LOW) {
                previous_level = current_level;
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

            for (int bit = 0; bit < 8; ++bit) {
                const uint32_t sample_us = start_us + 300000UL + static_cast<uint32_t>(bit) * 200000UL;
                while (static_cast<int32_t>(sample_us - micros()) > 0) {
                    delayMicroseconds(50);
                }
                if (digitalRead(PIN_KLINE_RX) == HIGH) {
                    value |= static_cast<uint8_t>(1U << bit);
                }
            }

            const uint32_t stop_sample_us = start_us + 1900000UL;
            while (static_cast<int32_t>(stop_sample_us - micros()) > 0) {
                delayMicroseconds(50);
            }

            const uint32_t idle_confirm_us = stop_sample_us + 100000UL;
            while (static_cast<int32_t>(idle_confirm_us - micros()) > 0) {
                delayMicroseconds(50);
            }

            const bool stop_high = digitalRead(PIN_KLINE_RX) == HIGH;
            if (stop_high && value == expected) {
                return true;
            }

            const uint32_t now_ms = millis();
            if ((now_ms - last_candidate_log_ms) > 250) {
                Serial.printf("[K-Line] Candidato 5-baud: 0x%02X (esperado 0x%02X, stop=%s)\n",
                              value,
                              expected,
                              stop_high ? "HIGH" : "LOW");
                last_candidate_log_ms = now_ms;
            }
        }

        previous_level = current_level;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return false;
}

static bool wait_for_uart_byte(uint8_t& value, uint32_t timeout_ms) {
    const int read = uart_read_bytes(kline_port(), &value, 1, pdMS_TO_TICKS(timeout_ms));
    return read == 1;
}

static bool wait_for_fast_init_pulse(uint32_t timeout_ms) {
    prepare_kline_idle();

    const uint32_t deadline_ms = millis() + timeout_ms;
    int previous_level = digitalRead(PIN_KLINE_RX);
    uint32_t edge_us = micros();
    uint32_t last_yield_us = micros();

    while (millis() < deadline_ms) {
        const int current_level = digitalRead(PIN_KLINE_RX);
        if (current_level != previous_level) {
            const uint32_t now_us = micros();
            const uint32_t elapsed_us = now_us - edge_us;

            if (previous_level == HIGH && current_level == LOW) {
                edge_us = now_us;
            } else if (previous_level == LOW && current_level == HIGH) {
                if (elapsed_us >= 18000UL && elapsed_us <= 35000UL) {
                    const uint32_t high_deadline = micros() + 35000UL;
                    while (static_cast<int32_t>(high_deadline - micros()) > 0) {
                        if (digitalRead(PIN_KLINE_RX) == LOW) {
                            break;
                        }
                        delayMicroseconds(50);
                    }
                    const uint32_t high_us = micros() - now_us;
                    if (digitalRead(PIN_KLINE_RX) == HIGH && high_us >= 18000UL) {
                        Serial.printf("[K-Line] Fast init detectado (LOW=%lu us, HIGH=%lu us)\n",
                                      static_cast<unsigned long>(elapsed_us),
                                      static_cast<unsigned long>(high_us));
                        return true;
                    }
                }
                edge_us = now_us;
            } else {
                edge_us = now_us;
            }

            previous_level = current_level;
        }

        const uint32_t now_us = micros();
        if (now_us - last_yield_us >= 2000UL) {
            vTaskDelay(pdMS_TO_TICKS(1));
            last_yield_us = micros();
        } else {
            delayMicroseconds(50);
        }
    }

    return false;
}

static bool uart_write_byte_blocking(uint8_t value, uint32_t timeout_ms = 50) {
    const int written = uart_write_bytes(kline_port(), reinterpret_cast<const char*>(&value), 1);
    if (written != 1) {
        return false;
    }
    return uart_wait_tx_done(kline_port(), pdMS_TO_TICKS(timeout_ms)) == ESP_OK;
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

static bool is_kwp_protocol(uint8_t protocol) {
    return protocol == PROTO_KWP_5BAUD || protocol == PROTO_KWP_FAST;
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

static bool is_response_mode_byte(uint8_t mode) {
    return mode == 0x7F || (mode >= 0x40 && mode <= 0x7F);
}

static void send_kline_response(const OBDResponse& resp, const OBDRequest& req) {
    uint8_t payload[80] = {};
    const uint8_t payload_len = build_kline_payload(resp, req, payload, sizeof(payload));
    if (payload_len == 0) {
        return;
    }

    uint8_t frame[96] = {};
    uint8_t len = 0;
    if (is_kwp_protocol(s_running_proto)) {
        frame[len++] = static_cast<uint8_t>(0x80 | payload_len);
        frame[len++] = KLINE_TESTER_ADDR;
        frame[len++] = KLINE_ECU_ADDR;
    } else {
        frame[len++] = 0x48;
        frame[len++] = 0x6B;
        frame[len++] = KLINE_ECU_ADDR;
    }
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

    if (checksum(buf, len - 1) != buf[len - 1]) {
        return ParseResult::INCOMPLETE;
    }

    const uint8_t payload_len = len - 4;
    if (payload_len == 0) {
        return ParseResult::INVALID;
    }

    if (is_response_mode_byte(buf[3])) {
        return ParseResult::UNSUPPORTED;
    }

    req.mode = buf[3];
    req.pid = 0x00;
    req.data_len = 0;

    if (payload_len == 1) {
        return ParseResult::OK;
    }

    req.pid = buf[4];
    if (payload_len > 2) {
        req.data_len = payload_len - 2;
        if (req.data_len > sizeof(req.data)) {
            return ParseResult::INVALID;
        }
        memcpy(req.data, &buf[5], req.data_len);
    }

    return ParseResult::OK;
}

static void log_kline_frame_hex(const char* prefix, const uint8_t* buf, uint8_t len) {
    Serial.print(prefix);
    for (uint8_t i = 0; i < len; ++i) {
        Serial.printf("%s%02X", i == 0 ? "" : " ", buf[i]);
    }
    Serial.println();
}

static bool start_kline_protocol(uint8_t protocol) {
    if (protocol == PROTO_ISO9141 || protocol == PROTO_KWP_5BAUD) {
        Serial.printf("[K-Line] Aguardando 5-baud init para %s\n", protoName(protocol));

        if (!wait_for_5baud_address(0x33, 5000)) {
            Serial.println("[K-Line] Endereco 0x33 nao detectado");
            return false;
        }
        Serial.println("[K-Line] Endereco 0x33 detectado");

        if (!uart_kline_begin()) {
            return false;
        }

        const uint8_t kb1 = (protocol == PROTO_ISO9141) ? 0x08 : 0xEF;
        const uint8_t kb2 = (protocol == PROTO_ISO9141) ? 0x08 : 0x8F;
        vTaskDelay(pdMS_TO_TICKS(25));

        if (!uart_kline_reconfigure(UART_DATA_8_BITS, UART_PARITY_DISABLE)) {
            Serial.println("[K-Line] Falha ao configurar UART 8N1");
            uart_kline_end();
            return false;
        }
        if (!uart_write_byte_blocking(0x55, 100)) {
            Serial.println("[K-Line] Falha ao transmitir sync 0x55");
            uart_kline_end();
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
        if (!uart_kline_reconfigure(UART_DATA_8_BITS, UART_PARITY_ODD)) {
            Serial.println("[K-Line] Falha ao configurar UART com paridade");
            uart_kline_end();
            return false;
        }
        if (!uart_write_byte_blocking(kb1) || !uart_write_byte_blocking(kb2)) {
            Serial.println("[K-Line] Falha ao transmitir key bytes");
            uart_kline_end();
            return false;
        }
        Serial.printf("[K-Line] Keywords enviados: 0x%02X 0x%02X\n", kb1, kb2);

        uint8_t tester_complement = 0;
        if (wait_for_uart_byte(tester_complement, 300)) {
            Serial.printf("[K-Line] Complemento recebido: 0x%02X\n", tester_complement);
        } else {
            Serial.println("[K-Line] Complemento KB2 nao recebido");
            uart_kline_end();
            return false;
        }

        if (!uart_kline_reconfigure(UART_DATA_8_BITS, UART_PARITY_DISABLE)) {
            Serial.println("[K-Line] Falha ao retornar UART 8N1");
            uart_kline_end();
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(25));
        const uint8_t address_complement = (protocol == PROTO_ISO9141)
            ? static_cast<uint8_t>(KLINE_FUNC_ADDR ^ 0xFF)
            : static_cast<uint8_t>(KLINE_ECU_ADDR ^ 0xFF);
        if (!uart_write_byte_blocking(address_complement, 100)) {
            Serial.println("[K-Line] Falha ao transmitir complemento de endereco");
            uart_kline_end();
            return false;
        }
        Serial.printf("[K-Line] Complemento de endereco 0x%02X enviado\n", address_complement);

        uart_flush_input(kline_port());
    } else if (protocol == PROTO_KWP_FAST) {
        Serial.println("[K-Line] Aguardando fast init para KWP Fast");
        if (!wait_for_fast_init_pulse(5000)) {
            Serial.println("[K-Line] Fast init nao detectado");
            return false;
        }
        if (!uart_kline_begin()) {
            return false;
        }
        Serial.println("[K-Line] UART pronta para KWP Fast");
    }

    Serial.printf("[K-Line] Init OK - %s\n", protoName(protocol));
    s_last_activity_ms = millis();
    return true;
}

static void task_kline(void*) {
    uint8_t rx_buf[24] = {};
    uint8_t rx_len = 0;

    while (true) {
        uint8_t active_proto = 0;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        active_proto = s_state->active_protocol;
        xSemaphoreGive(s_mutex);

        if (!is_kline_protocol(active_proto)) {
            if (s_started) {
                uart_kline_end();
                s_started = false;
                s_running_proto = 0xFF;
                rx_len = 0;
                digitalWrite(PIN_LED_KLINE, LOW);
                Serial.println("[K-Line] UART parada");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!s_started || s_running_proto != active_proto) {
            if (s_started) {
                uart_kline_end();
                s_started = false;
                rx_len = 0;
            }
            if (!start_kline_protocol(active_proto)) {
                digitalWrite(PIN_LED_KLINE, LOW);
                Serial.printf("[K-Line] Falha ao iniciar UART para %s\n", protoName(active_proto));
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            s_started = true;
            s_running_proto = active_proto;
            s_last_activity_ms = millis();
            digitalWrite(PIN_LED_KLINE, HIGH);
        }

        uint8_t byte = 0;
        int read = uart_read_bytes(kline_port(), &byte, 1, pdMS_TO_TICKS(50));
        if (read <= 0) {
            if (s_started && (millis() - s_last_activity_ms) > KLINE_SESSION_IDLE_TIMEOUT_MS) {
                Serial.println("[K-Line] Sessao expirada por inatividade; aguardando novo init");
                uart_kline_end();
                s_started = false;
                s_running_proto = 0xFF;
                rx_len = 0;
                continue;
            }
            rx_len = 0;
            continue;
        }

        s_last_activity_ms = millis();

        if (rx_len < sizeof(rx_buf)) {
            rx_buf[rx_len++] = byte;
        }

        OBDRequest req = {};
        ParseResult parse = parse_kline_frame(rx_buf, rx_len, req);
        if (parse == ParseResult::INCOMPLETE) {
            continue;
        }
        if (parse != ParseResult::OK) {
            if (is_kwp_protocol(s_running_proto)) {
                if (parse == ParseResult::INVALID) {
                    log_kline_frame_hex("[K-Line] Frame invalido: ", rx_buf, rx_len);
                } else if (parse == ParseResult::UNSUPPORTED) {
                    log_kline_frame_hex("[K-Line] Frame ignorado: ", rx_buf, rx_len);
                }
            }
            rx_len = 0;
            continue;
        }

        if (is_kwp_protocol(s_running_proto)) {
            Serial.printf("[K-Line] Req KWP mode=0x%02X pid=0x%02X len=%u\n",
                          req.mode, req.pid, req.data_len);
        }

        rx_len = 0;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        OBDResponse resp;
        if (is_kwp_protocol(s_running_proto) && req.mode == 0x81) {
            resp.data[resp.len++] = 0xC1;
            resp.data[resp.len++] = 0xEF;
            resp.data[resp.len++] = 0x8F;
        } else if (is_kwp_protocol(s_running_proto) && req.mode == 0x3E) {
            resp.data[resp.len++] = 0x7E;
        } else {
            resp = s_dispatcher.dispatch(req, *s_state);
            if (req.mode == 0x04) {
                diagnostic_engine_handle_mode04_clear(*s_state);
            }
        }
        xSemaphoreGive(s_mutex);

        if (is_kwp_protocol(s_running_proto)) {
            Serial.printf("[K-Line] Resp KWP len=%u neg=%s\n",
                          resp.len,
                          resp.negative ? "true" : "false");
        }
        send_kline_response(resp, req);
        s_last_activity_ms = millis();
    }
}

void kline_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(task_kline, "task_kline", 6144, nullptr, 10, nullptr, 0);
}
