#include "mode02_handler.h"

#include <Arduino.h>

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

static uint8_t encode_trim(float pct) {
    float raw = (pct * 128.0f / 100.0f) + 128.0f;
    if (raw < 0.0f) raw = 0.0f;
    if (raw > 255.0f) raw = 255.0f;
    return static_cast<uint8_t>(raw);
}

static uint8_t encode_load(uint8_t pct) {
    return static_cast<uint8_t>((static_cast<uint32_t>(pct) * 255u) / 100u);
}

static uint8_t encode_tps(uint8_t pct) {
    return static_cast<uint8_t>((static_cast<uint32_t>(pct) * 255u) / 100u);
}

static void encode_u16(uint8_t* out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value >> 8);
    out[1] = static_cast<uint8_t>(value & 0xFF);
}

static uint8_t clamp_u8_int(int value, int lo, int hi) {
    if (value < lo) return static_cast<uint8_t>(lo);
    if (value > hi) return static_cast<uint8_t>(hi);
    return static_cast<uint8_t>(value);
}

static uint16_t derived_fuel_pressure_kpa(const DiagnosticFreezeFrame& ff) {
    const int value = 180 + static_cast<int>(ff.map_kpa) * 3 + static_cast<int>(ff.engine_load_pct) * 2;
    if (value < 30) return 30;
    if (value > 765) return 765;
    return static_cast<uint16_t>(value);
}

OBDResponse Mode02Handler::handle(const OBDRequest& req, const SimulationState&) {
    if (req.data_len < 1) {
        return negResp(0x13);
    }

    const uint8_t frame = req.data[0];
    if (frame != 0x00) {
        return negResp(0x31);
    }

    DiagnosticFreezeFrame ff = {};
    const bool has_snapshot = diagnostic_get_freeze_frame(ff);

    if (!has_snapshot) {
        if (req.pid == 0x02) {
            RESP(0x00, 0x00);
        }
        return negResp(0x22);
    }

    switch (req.pid) {
        case 0x02:
            RESP(static_cast<uint8_t>(ff.dtc >> 8), static_cast<uint8_t>(ff.dtc & 0xFF));

        case 0x04:
            RESP(encode_load(ff.engine_load_pct));

        case 0x05:
            RESP(static_cast<uint8_t>(ff.coolant_temp_c + 40));

        case 0x06:
            RESP(encode_trim(ff.stft_pct));

        case 0x07:
            RESP(encode_trim(ff.ltft_pct));

        case 0x08:
            RESP(encode_trim(ff.stft_pct));

        case 0x09:
            RESP(encode_trim(ff.ltft_pct));

        case 0x0A: {
            const uint16_t pressure = derived_fuel_pressure_kpa(ff);
            RESP(static_cast<uint8_t>(pressure / 3));
        }

        case 0x0B:
            RESP(ff.map_kpa);

        case 0x0C: {
            uint8_t raw[2] = {};
            encode_u16(raw, static_cast<uint16_t>(ff.rpm * 4u));
            RESP(raw[0], raw[1]);
        }

        case 0x0D:
            RESP(ff.speed_kmh);

        case 0x0E: {
            const float raw = (ff.ignition_adv + 64.0f) * 2.0f;
            RESP(clamp_u8_int(static_cast<int>(raw + 0.5f), 0, 255));
        }

        case 0x0F:
            RESP(static_cast<uint8_t>(ff.intake_temp_c + 40));

        case 0x10: {
            uint8_t raw[2] = {};
            encode_u16(raw, static_cast<uint16_t>(ff.maf_gs * 100.0f));
            RESP(raw[0], raw[1]);
        }

        case 0x11:
            RESP(encode_tps(ff.throttle_pct));

        case 0x2F:
            RESP(encode_tps(ff.fuel_level_pct));

        case 0x33:
            RESP(100);

        case 0x42: {
            uint8_t raw[2] = {};
            encode_u16(raw, static_cast<uint16_t>(ff.battery_voltage * 1000.0f));
            RESP(raw[0], raw[1]);
        }

        case 0x46:
            RESP(clamp_u8_int(ff.intake_temp_c - 5 + 40, 0, 255));

        default:
            return negResp(0x12);
    }
}
