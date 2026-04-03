#pragma once

#include <cstddef>
#include <cstdint>
#include "simulation_state.h"

enum DiagnosticScenarioId : uint8_t {
    DIAG_SCENARIO_NONE = 0,
    DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE = 1,
    DIAG_SCENARIO_URBAN_OVERHEAT_FAN_DEGRADED = 2,
    DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK = 3,
    DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP = 4,
    DIAG_SCENARIO_UNSTABLE_VOLTAGE_MULTI_MODULE = 5,
    DIAG_SCENARIO_WHEEL_SPEED_ABS_INCONSISTENT = 6,
};

constexpr uint8_t DIAG_SCENARIO_MAX_FAULTS = 6;

struct ScenarioFaultSpec {
    uint8_t fault_id = 0;
    uint16_t start_delay_ticks = 0;
    uint16_t onset_ticks = 0;
    uint16_t aggravating_ticks = 0;
    uint16_t intermittent_ticks = 0;
    uint16_t persistent_ticks = 0;
    uint16_t recovering_ticks = 0;
    uint8_t max_intensity_pct = 0;
};

struct ScenarioDefinition {
    DiagnosticScenarioId id = DIAG_SCENARIO_NONE;
    const char* slug = "";
    const char* label = "";
    const char* summary = "";
    SimMode default_mode = SIM_STATIC;
    uint8_t allowed_mode_mask = 0;
    uint8_t fault_count = 0;
    ScenarioFaultSpec faults[DIAG_SCENARIO_MAX_FAULTS] = {};
};

constexpr uint8_t sim_mode_mask(SimMode mode) {
    return static_cast<uint8_t>(1u << static_cast<uint8_t>(mode));
}

const ScenarioDefinition* diagnostic_scenario_get(DiagnosticScenarioId id);
const ScenarioDefinition* diagnostic_scenario_all(size_t& count);
DiagnosticScenarioId diagnostic_scenario_from_slug(const char* slug);
const char* diagnostic_scenario_slug(DiagnosticScenarioId id);
const char* diagnostic_scenario_label(DiagnosticScenarioId id);
bool diagnostic_scenario_allows_mode(DiagnosticScenarioId id, SimMode mode);
