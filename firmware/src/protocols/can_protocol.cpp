#include "can_protocol.h"
#include "config.h"
#include "diagnostic_scenario_engine.h"
#include <Arduino.h>
#include <driver/twai.h>
#include <cstring>

static OBDDispatcher s_dispatcher;
static SimulationState* s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

static bool is_can_protocol(uint8_t protocol) {
    return protocol <= PROTO_CAN_29B_250K;
}

static bool is_extended_protocol(uint8_t protocol) {
    return protocol == PROTO_CAN_29B_500K || protocol == PROTO_CAN_29B_250K;
}

static bool is_obd_request_id(const twai_message_t& msg, uint8_t protocol) {
    if (is_extended_protocol(protocol)) {
        return msg.extd != 0 &&
               (msg.identifier == OBD_CAN_FUNC_29B || msg.identifier == 0x18DA10F1);
    }

    return msg.extd == 0 &&
           (msg.identifier == OBD_CAN_FUNC_11B || msg.identifier == OBD_CAN_PHYS_11B);
}

static bool twai_begin_protocol(uint8_t protocol) {
    twai_general_config_t general =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, TWAI_MODE_NORMAL);
    general.tx_queue_len = 10;
    general.rx_queue_len = 20;

    twai_timing_config_t timing;
    if (protocol == PROTO_CAN_11B_500K || protocol == PROTO_CAN_29B_500K) {
        timing = TWAI_TIMING_CONFIG_500KBITS();
    } else {
        timing = TWAI_TIMING_CONFIG_250KBITS();
    }

    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
        return false;
    }
    if (twai_start() != ESP_OK) {
        twai_driver_uninstall();
        return false;
    }
    return true;
}

static void twai_end_protocol() {
    twai_stop();
    twai_driver_uninstall();
}

static ParseResult parse_frame(const twai_message_t& msg, OBDRequest& req) {
    if (msg.data_length_code < 2) {
        return ParseResult::INVALID;
    }

    const uint8_t pci = msg.data[0];
    const uint8_t frame_type = (pci >> 4) & 0x0F;
    if (frame_type != 0x0) {
        return ParseResult::UNSUPPORTED;
    }

    const uint8_t payload_len = pci & 0x0F;
    if (payload_len < 1 || payload_len > 7) {
        return ParseResult::INVALID;
    }

    req.mode = msg.data[1];
    req.pid = (payload_len >= 2) ? msg.data[2] : 0x00;
    req.data_len = 0;

    if (payload_len > 2) {
        req.data_len = payload_len - 2;
        if (req.data_len > sizeof(req.data)) {
            return ParseResult::INVALID;
        }
        for (uint8_t i = 0; i < req.data_len; i++) {
            req.data[i] = msg.data[3 + i];
        }
    }

    return ParseResult::OK;
}

static uint8_t build_can_payload(const OBDResponse& resp, const OBDRequest& req, uint8_t* payload, size_t capacity) {
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

static bool transmit_can_obd_frame(const uint8_t* bytes, size_t len, bool extended) {
    twai_message_t tx = {};
    tx.extd = extended ? 1 : 0;
    tx.identifier = extended ? OBD_CAN_RESP_29B : OBD_CAN_RESP_11B;
    tx.data_length_code = 8;
    memset(tx.data, 0, sizeof(tx.data));
    memcpy(tx.data, bytes, len > sizeof(tx.data) ? sizeof(tx.data) : len);
    return twai_transmit(&tx, pdMS_TO_TICKS(20)) == ESP_OK;
}

static uint32_t flow_control_delay_ms(uint8_t st_min) {
    if (st_min <= 0x7F) {
        return st_min;
    }
    return 0;
}

static bool wait_flow_control(bool extended, uint8_t& block_size, uint8_t& st_min) {
    const uint32_t deadline = millis() + 400;

    while (millis() < deadline) {
        twai_message_t rx = {};
        if (twai_receive(&rx, pdMS_TO_TICKS(50)) != ESP_OK) {
            continue;
        }
        if ((rx.extd != 0) != extended || rx.data_length_code == 0) {
            continue;
        }
        const uint8_t type = (rx.data[0] >> 4) & 0x0F;
        if (type != 0x3) {
            continue;
        }
        const uint8_t fs = rx.data[0] & 0x0F;
        if (fs != 0x0) {
            return false;
        }
        block_size = rx.data[1];
        st_min = rx.data[2];
        return true;
    }

    return false;
}

static void blink_tx_led() {
    digitalWrite(PIN_LED_TX, HIGH);
    vTaskDelay(pdMS_TO_TICKS(30));
    digitalWrite(PIN_LED_TX, LOW);
}

static void send_response(const OBDResponse& resp, const OBDRequest& req, bool extended) {
    uint8_t payload[80] = {};
    const uint8_t payload_len = build_can_payload(resp, req, payload, sizeof(payload));
    if (payload_len == 0) {
        return;
    }

    if (payload_len <= 7) {
        uint8_t sf[8] = {};
        sf[0] = payload_len;
        memcpy(&sf[1], payload, payload_len);
        if (transmit_can_obd_frame(sf, sizeof(sf), extended)) {
            blink_tx_led();
        }
        return;
    }

    uint8_t ff[8] = {};
    ff[0] = 0x10 | ((payload_len >> 8) & 0x0F);
    ff[1] = payload_len & 0xFF;
    memcpy(&ff[2], payload, 6);
    if (!transmit_can_obd_frame(ff, sizeof(ff), extended)) {
        return;
    }

    uint8_t block_size = 0;
    uint8_t st_min = 0;
    if (!wait_flow_control(extended, block_size, st_min)) {
        Serial.println("[CAN] Flow Control nao recebido");
        return;
    }

    size_t offset = 6;
    uint8_t seq = 1;
    uint8_t sent_in_block = 0;
    const uint32_t delay_ms = flow_control_delay_ms(st_min);

    while (offset < payload_len) {
        uint8_t cf[8] = {};
        cf[0] = 0x20 | (seq & 0x0F);
        const size_t chunk = (payload_len - offset) > 7 ? 7 : (payload_len - offset);
        memcpy(&cf[1], &payload[offset], chunk);
        if (!transmit_can_obd_frame(cf, sizeof(cf), extended)) {
            return;
        }

        offset += chunk;
        seq = (seq + 1) & 0x0F;
        sent_in_block++;

        if (delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }

        if (block_size != 0 && sent_in_block >= block_size && offset < payload_len) {
            sent_in_block = 0;
            if (!wait_flow_control(extended, block_size, st_min)) {
                Serial.println("[CAN] Novo Flow Control nao recebido");
                return;
            }
        }
    }

    blink_tx_led();
}

static void task_can(void*) {
    bool started = false;
    uint8_t running_proto = 0xFF;

    while (true) {
        uint8_t active_proto = 0;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        active_proto = s_state->active_protocol;
        xSemaphoreGive(s_mutex);

        if (!is_can_protocol(active_proto)) {
            if (started) {
                twai_end_protocol();
                started = false;
                running_proto = 0xFF;
                Serial.println("[CAN] TWAI parado");
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!started || running_proto != active_proto) {
            if (started) {
                twai_end_protocol();
                started = false;
            }
            if (!twai_begin_protocol(active_proto)) {
                Serial.printf("[CAN] Falha ao iniciar TWAI para %s\n", protoName(active_proto));
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            started = true;
            running_proto = active_proto;
            Serial.printf("[CAN] TWAI iniciado - %s\n", protoName(running_proto));
        }

        twai_message_t rx = {};
        if (twai_receive(&rx, pdMS_TO_TICKS(100)) != ESP_OK) {
            continue;
        }
        if (!is_obd_request_id(rx, running_proto)) {
            continue;
        }

        OBDRequest req = {};
        if (parse_frame(rx, req) != ParseResult::OK) {
            continue;
        }

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        OBDResponse resp = s_dispatcher.dispatch(req, *s_state);
        if (req.mode == 0x04) {
            diagnostic_engine_handle_mode04_clear(*s_state);
        }
        xSemaphoreGive(s_mutex);

        send_response(resp, req, is_extended_protocol(running_proto));
    }
}

void can_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(task_can, "task_can", 6144, nullptr, 10, nullptr, 0);
}
