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
    // Padding: resposta Mode 03 deve ter comprimento par (sem contar o byte count).
    // Se dtc_count for ímpar, adiciona 0x0000 extra para completar par de bytes.
    if ((s.dtc_count & 0x01) && r.len < (uint8_t)sizeof(r.data) - 1) {
        r.data[r.len++] = 0x00;
        r.data[r.len++] = 0x00;
    }
    return r;
}
