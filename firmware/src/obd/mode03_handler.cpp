#include "mode03_handler.h"

// ════════════════════════════════════════════════════════════
//  Mode 03 — Request Emission-Related DTCs
// ════════════════════════════════════════════════════════════

OBDResponse Mode03Handler::handle(const SimulationState& s) {
    OBDResponse r;
    r.data[r.len++] = s.dtc_count;
    for (uint8_t i = 0; i < s.dtc_count; i++) {
        r.data[r.len++] = (uint8_t)(s.dtcs[i] >> 8);   // byte alto do DTC
        r.data[r.len++] = (uint8_t)(s.dtcs[i] & 0xFF); // byte baixo
    }
    // padding se necessário para completar múltiplos de 2
    return r;
}
