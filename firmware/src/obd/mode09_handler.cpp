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
        // Formato: [count=0x01] + exatamente 17 bytes ASCII do VIN
        // VINs menores que 17 bytes são padded com 0x00; maiores são truncados.
        r.data[r.len++] = 0x01; // message count
        for (uint8_t i = 0; i < 17 && r.len < (uint8_t)sizeof(r.data); i++) {
            r.data[r.len++] = (i < strlen(s.vin)) ? (uint8_t)s.vin[i] : 0x00;
        }
        return r;
    }

    r.negative = true;
    r.nrc = 0x12; // requestOutOfRange
    return r;
}
