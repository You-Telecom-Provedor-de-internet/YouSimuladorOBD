#include "mode01_handler.h"
#include <Arduino.h>

// ════════════════════════════════════════════════════════════
//  Mode 01 — Current Data (dados correntes do veículo)
// ════════════════════════════════════════════════════════════

static OBDResponse buildResp(const uint8_t* bytes, uint8_t count) {
    OBDResponse r;
    for (uint8_t i = 0; i < count; i++) r.data[r.len++] = bytes[i];
    return r;
}
// Macro para passar array literal
#define RESP(...) do { const uint8_t _b[] = {__VA_ARGS__}; return buildResp(_b, sizeof(_b)); } while(0)

static OBDResponse negResp(uint8_t nrc) {
    OBDResponse r;
    r.negative = true;
    r.nrc = nrc;
    return r;
}

OBDResponse Mode01Handler::handle(const OBDRequest& req, const SimulationState& s) {
    const uint8_t pid = req.pid;

    switch (pid) {

        // ── PIDs suportados 0x01–0x20 ─────────────────────────
        case 0x00:
            RESP(0x18, 0x3F, 0x80, 0x01);

        case 0x20:
            RESP(0x00, 0x02, 0x00, 0x01);

        case 0x04: {
            uint8_t a = (uint8_t)((uint32_t)s.engine_load_pct * 255 / 100);
            RESP(a);
        }
        case 0x05: {
            uint8_t a = (uint8_t)(s.coolant_temp_c + 40);
            RESP(a);
        }
        case 0x0B:
            RESP(s.map_kpa);

        case 0x0C: {
            uint16_t raw = (uint16_t)(s.rpm * 4);
            RESP((uint8_t)(raw >> 8), (uint8_t)(raw & 0xFF));
        }
        case 0x0D:
            RESP(s.speed_kmh);

        case 0x0E: {
            uint8_t a = (uint8_t)((s.ignition_adv + 64.0f) * 2.0f);
            RESP(a);
        }
        case 0x0F: {
            uint8_t a = (uint8_t)(s.intake_temp_c + 40);
            RESP(a);
        }
        case 0x10: {
            uint16_t raw = (uint16_t)(s.maf_gs * 100.0f);
            RESP((uint8_t)(raw >> 8), (uint8_t)(raw & 0xFF));
        }
        case 0x11: {
            uint8_t a = (uint8_t)((uint32_t)s.throttle_pct * 255 / 100);
            RESP(a);
        }
        case 0x2F: {
            uint8_t a = (uint8_t)((uint32_t)s.fuel_level_pct * 255 / 100);
            RESP(a);
        }

        default:
            return negResp(0x12); // requestOutOfRange
    }
}
