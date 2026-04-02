#include "mode03_handler.h"

// ════════════════════════════════════════════════════════════
//  Mode 03 — Request Emission-Related DTCs
// ════════════════════════════════════════════════════════════

OBDResponse Mode03Handler::handle(const SimulationState& s) {
    OBDResponse r;
    // Limita DTCs ao que cabe no buffer (20 bytes): 1 count + 2*N ≤ 19 → max 9
    uint8_t max_dtcs = (sizeof(r.data) - 1) / 2; // 9
    uint8_t count = (s.dtc_count <= max_dtcs) ? s.dtc_count : max_dtcs;
    r.data[r.len++] = count;
    for (uint8_t i = 0; i < count; i++) {
        r.data[r.len++] = (uint8_t)(s.dtcs[i] >> 8);
        r.data[r.len++] = (uint8_t)(s.dtcs[i] & 0xFF);
    }
    return r;
}
