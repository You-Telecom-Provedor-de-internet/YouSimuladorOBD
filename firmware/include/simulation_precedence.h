#pragma once

#include <cstdint>

enum SimulationModeSource : uint8_t {
    SIM_MODE_SOURCE_BOOT = 0,
    SIM_MODE_SOURCE_USER = 1,
    SIM_MODE_SOURCE_SCENARIO = 2,
    SIM_MODE_SOURCE_PRESET = 3,
};

enum SimulationNoticeCode : uint8_t {
    SIM_NOTICE_NONE = 0,
    SIM_NOTICE_PROFILE_APPLIED = 1,
    SIM_NOTICE_PROFILE_RESTORED = 2,
    SIM_NOTICE_PROFILE_CLEARED_BY_PROTOCOL = 3,
    SIM_NOTICE_PROTOCOL_CHANGED = 4,
    SIM_NOTICE_PRESET_APPLIED = 5,
    SIM_NOTICE_SCENARIO_APPLIED = 6,
    SIM_NOTICE_SCENARIO_FORCED_MODE = 7,
    SIM_NOTICE_SCENARIO_CLEARED_BY_MODE = 8,
    SIM_NOTICE_SCENARIO_CLEARED_BY_MANUAL_EDIT = 9,
    SIM_NOTICE_SCENARIO_CLEARED = 10,
};

struct SimulationPrecedenceState {
    uint8_t sim_mode_source = SIM_MODE_SOURCE_BOOT;
    uint8_t notice_code = SIM_NOTICE_NONE;
};

void simulation_precedence_reset();
void simulation_precedence_set_mode_source(SimulationModeSource source);
void simulation_precedence_set_notice(SimulationNoticeCode code);
SimulationPrecedenceState simulation_precedence_snapshot();
