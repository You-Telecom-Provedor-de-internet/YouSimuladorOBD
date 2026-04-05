#include "simulation_precedence.h"

static SimulationPrecedenceState s_precedence_state = {};

void simulation_precedence_reset() {
    s_precedence_state = {};
}

void simulation_precedence_set_mode_source(SimulationModeSource source) {
    s_precedence_state.sim_mode_source = static_cast<uint8_t>(source);
}

void simulation_precedence_set_notice(SimulationNoticeCode code) {
    s_precedence_state.notice_code = static_cast<uint8_t>(code);
}

SimulationPrecedenceState simulation_precedence_snapshot() {
    return s_precedence_state;
}
