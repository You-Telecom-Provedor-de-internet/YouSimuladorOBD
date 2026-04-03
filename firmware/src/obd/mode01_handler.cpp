#include "mode01_handler.h"
#include <Arduino.h>

// Mode 01 - Current Data (dados correntes do veiculo)

static OBDResponse buildResp(const uint8_t* bytes, uint8_t count) {
    OBDResponse r;
    for (uint8_t i = 0; i < count; i++) {
        r.data[r.len++] = bytes[i];
    }
    return r;
}

#define RESP(...) do { const uint8_t _b[] = {__VA_ARGS__}; return buildResp(_b, sizeof(_b)); } while(0)

static OBDResponse negResp(uint8_t nrc) {
    OBDResponse r;
    r.negative = true;
    r.nrc = nrc;
    return r;
}

static OBDResponse supportedPidResp21to40() {
    // 0x21, 0x2F, 0x31, 0x40
    RESP(0x80, 0x02, 0x80, 0x01);
}

static OBDResponse supportedPidResp41to60(const SimulationState& s) {
    // 0x42, 0x5C e opcionalmente 0x60 como ponte para a cadeia do A6.
    uint8_t d = 0x10;
    if (s.pid_a6_supported) {
        d |= 0x01;
    }
    RESP(0x40, 0x00, 0x00, d);
}

OBDResponse Mode01Handler::handle(const OBDRequest& req, const SimulationState& s) {
    const uint8_t pid = req.pid;

    switch (pid) {
        case 0x00:
            // 0x04,0x05,0x06,0x07,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x20
            RESP(0x1E, 0x3F, 0x80, 0x01);

        case 0x20:
            return supportedPidResp21to40();

        case 0x40:
            return supportedPidResp41to60(s);

        case 0x60:
            if (!s.pid_a6_supported) return negResp(0x12);
            RESP(0x00, 0x00, 0x00, 0x01); // 0x80

        case 0x80:
            if (!s.pid_a6_supported) return negResp(0x12);
            RESP(0x00, 0x00, 0x00, 0x01); // 0xA0

        case 0xA0:
            if (!s.pid_a6_supported) return negResp(0x12);
            RESP(0x04, 0x00, 0x00, 0x00); // 0xA6

        case 0x04: {
            uint8_t a = static_cast<uint8_t>((static_cast<uint32_t>(s.engine_load_pct) * 255u) / 100u);
            RESP(a);
        }
        case 0x05:
            RESP(static_cast<uint8_t>(s.coolant_temp_c + 40));

        case 0x06: {
            uint8_t a = static_cast<uint8_t>((s.stft_pct * 128.0f / 100.0f) + 128.0f);
            RESP(a);
        }
        case 0x07: {
            uint8_t a = static_cast<uint8_t>((s.ltft_pct * 128.0f / 100.0f) + 128.0f);
            RESP(a);
        }
        case 0x0B:
            RESP(s.map_kpa);

        case 0x0C: {
            uint16_t raw = static_cast<uint16_t>(s.rpm * 4u);
            RESP(static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF));
        }
        case 0x0D:
            RESP(s.speed_kmh);

        case 0x0E: {
            uint8_t a = static_cast<uint8_t>((s.ignition_adv + 64.0f) * 2.0f);
            RESP(a);
        }
        case 0x0F:
            RESP(static_cast<uint8_t>(s.intake_temp_c + 40));

        case 0x10: {
            uint16_t raw = static_cast<uint16_t>(s.maf_gs * 100.0f);
            RESP(static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF));
        }
        case 0x11: {
            uint8_t a = static_cast<uint8_t>((static_cast<uint32_t>(s.throttle_pct) * 255u) / 100u);
            RESP(a);
        }
        case 0x21:
            RESP(static_cast<uint8_t>(s.distance_mil_on_km >> 8), static_cast<uint8_t>(s.distance_mil_on_km & 0xFF));

        case 0x2F: {
            uint8_t a = static_cast<uint8_t>((static_cast<uint32_t>(s.fuel_level_pct) * 255u) / 100u);
            RESP(a);
        }
        case 0x31:
            RESP(static_cast<uint8_t>(s.distance_since_clear_km >> 8), static_cast<uint8_t>(s.distance_since_clear_km & 0xFF));

        case 0x42: {
            uint16_t raw = static_cast<uint16_t>(s.battery_voltage * 1000.0f);
            RESP(static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF));
        }
        case 0x5C:
            RESP(static_cast<uint8_t>(s.oil_temp_c + 40));

        case 0xA6: {
            if (!s.pid_a6_supported) return negResp(0x12);
            const uint32_t raw = s.odometer_total_km_x10;
            RESP(static_cast<uint8_t>(raw >> 24),
                 static_cast<uint8_t>((raw >> 16) & 0xFF),
                 static_cast<uint8_t>((raw >> 8) & 0xFF),
                 static_cast<uint8_t>(raw & 0xFF));
        }

        default:
            return negResp(0x12);
    }
}
