#include "mode01_handler.h"
#include <Arduino.h>

// Mode 01 - Current Data (dados correntes do veiculo)
static uint32_t s_engine_start_ms = 0;
static bool s_engine_running = false;

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

static uint16_t runtime_since_engine_start_s(const SimulationState& s) {
    const bool running = s.rpm > 0;
    const uint32_t now = millis();

    if (!running) {
        s_engine_running = false;
        s_engine_start_ms = now;
        return 0;
    }

    if (!s_engine_running) {
        s_engine_running = true;
        s_engine_start_ms = now;
    }

    const uint32_t elapsed_s = (now - s_engine_start_ms) / 1000UL;
    return static_cast<uint16_t>(elapsed_s > 0xFFFFUL ? 0xFFFFUL : elapsed_s);
}

static OBDResponse monitorStatusResp(const SimulationState& s) {
    const uint8_t dtc_count = (s.dtc_count > 0x7F) ? 0x7F : s.dtc_count;
    const uint8_t byte_a = static_cast<uint8_t>((simulation_mil_active(s) ? 0x80 : 0x00) | dtc_count);

    // Spark ignition, common monitors supported and complete.
    const uint8_t byte_b = 0x07;
    // Catalyst, EVAP, O2 sensor, O2 heater, EGR/VVT available.
    const uint8_t byte_c = 0xE5;
    // All supported engine-specific monitors complete.
    const uint8_t byte_d = 0x00;

    RESP(byte_a, byte_b, byte_c, byte_d);
}

static OBDResponse fuelSystemStatusResp(const SimulationState& s) {
    uint8_t bank1 = 0x00;
    if (s.rpm == 0) {
        bank1 = 0x00;
    } else if (s.coolant_temp_c < 70) {
        bank1 = 0x01;
    } else if (simulation_mil_active(s)) {
        bank1 = 0x10;
    } else {
        bank1 = 0x02;
    }
    RESP(bank1, 0x00);
}

static OBDResponse freezeDtcResp(const SimulationState& s) {
    if (s.dtc_count == 0) {
        RESP(0x00, 0x00);
    }
    RESP(static_cast<uint8_t>(s.dtcs[0] >> 8), static_cast<uint8_t>(s.dtcs[0] & 0xFF));
}

static OBDResponse supportedPidResp21to40() {
    // 0x21, 0x2F, 0x31, 0x33, 0x40
    RESP(0x80, 0x02, 0xA0, 0x01);
}

static OBDResponse supportedPidResp41to60(const SimulationState& s) {
    // 0x42, 0x46, 0x51, 0x5C e opcionalmente 0x60 como ponte para a cadeia do A6.
    uint8_t d = 0x10;
    if (s.pid_a6_supported) {
        d |= 0x01;
    }
    RESP(0x44, 0x00, 0x80, d);
}

OBDResponse Mode01Handler::handle(const OBDRequest& req, const SimulationState& s) {
    const uint8_t pid = req.pid;

    switch (pid) {
        case 0x00:
            // 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x1C,0x1D,0x1E,0x1F,0x20
            RESP(0xFE, 0x3F, 0xE0, 0x1F);

        case 0x01:
            return monitorStatusResp(s);

        case 0x02:
            return freezeDtcResp(s);

        case 0x03:
            return fuelSystemStatusResp(s);

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
        case 0x12:
            RESP(0x00); // sem secondary air comandado

        case 0x13:
            RESP(0x02); // B1S1 presente

        case 0x1C:
            RESP(0x01); // OBD-II as defined by CARB

        case 0x1D:
            RESP(0x01); // localizacao simples de sensor O2

        case 0x1E:
            RESP(0x00); // PTO desligado

        case 0x1F: {
            const uint16_t runtime_s = runtime_since_engine_start_s(s);
            RESP(static_cast<uint8_t>(runtime_s >> 8), static_cast<uint8_t>(runtime_s & 0xFF));
        }

        case 0x21:
            RESP(static_cast<uint8_t>(s.distance_mil_on_km >> 8), static_cast<uint8_t>(s.distance_mil_on_km & 0xFF));

        case 0x2F: {
            uint8_t a = static_cast<uint8_t>((static_cast<uint32_t>(s.fuel_level_pct) * 255u) / 100u);
            RESP(a);
        }
        case 0x31:
            RESP(static_cast<uint8_t>(s.distance_since_clear_km >> 8), static_cast<uint8_t>(s.distance_since_clear_km & 0xFF));

        case 0x33:
            RESP(100); // barometric pressure kPa

        case 0x42: {
            uint16_t raw = static_cast<uint16_t>(s.battery_voltage * 1000.0f);
            RESP(static_cast<uint8_t>(raw >> 8), static_cast<uint8_t>(raw & 0xFF));
        }
        case 0x46:
            RESP(static_cast<uint8_t>(constrain(s.intake_temp_c - 5, -40, 215) + 40));

        case 0x51:
            RESP(0x01); // gasolina

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
