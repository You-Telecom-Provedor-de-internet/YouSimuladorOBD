#include "mode09_handler.h"
#include <cstring>

// ════════════════════════════════════════════════════════════
//  Mode 09 — Vehicle Information (VIN)
//  PID 0x02: VIN (17 bytes ASCII) — resposta multi-frame via ISO-TP
// ════════════════════════════════════════════════════════════

OBDResponse Mode09Handler::handle(const OBDRequest& req, const SimulationState& s) {
    OBDResponse r;

    if (req.pid == 0x00) {
        // PIDs suportados Mode 09: apenas PID 0x02 (VIN)
        // PID02 = bit 30 → 0x40 no byte 0
        r.data[r.len++] = 0x40;
        r.data[r.len++] = 0x00;
        r.data[r.len++] = 0x00;
        r.data[r.len++] = 0x00;
        return r;
    }

    if (req.pid == 0x02) {
        // Formato: [count=0x01] + 17 bytes ASCII do VIN
        r.data[r.len++] = 0x01; // message count
        size_t vlen = strlen(s.vin);
        for (size_t i = 0; i < vlen && r.len < sizeof(r.data); i++) {
            r.data[r.len++] = (uint8_t)s.vin[i];
        }
        return r;
    }

    r.negative = true;
    r.nrc = 0x12; // requestOutOfRange
    return r;
}
