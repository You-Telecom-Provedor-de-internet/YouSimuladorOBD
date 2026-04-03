#include "diagnostic_scenario_engine.h"

#include <cmath>
#include <cstring>

static float stage_factor(uint8_t stage) {
    switch (stage) {
        case FAULT_STAGE_ONSET: return 0.25f;
        case FAULT_STAGE_AGGRAVATING: return 0.55f;
        case FAULT_STAGE_INTERMITTENT: return 0.70f;
        case FAULT_STAGE_PERSISTENT: return 1.00f;
        case FAULT_STAGE_RECOVERING: return 0.35f;
        default: return 0.0f;
    }
}

static uint8_t alert_type_for_fault(uint8_t fault_id) {
    switch (fault_id) {
        case FAULT_ID_MISFIRE_MULTI: return DIAG_ALERT_PERFORMANCE_LOSS;
        case FAULT_ID_MISFIRE_CYL_1:
        case FAULT_ID_MISFIRE_CYL_4: return DIAG_ALERT_IGNITION_STABILITY;
        case FAULT_ID_FAN_DEGRADED:
        case FAULT_ID_COOLING_OVERTEMP: return DIAG_ALERT_COOLING_CRITICAL;
        case FAULT_ID_AIR_LEAK_IDLE: return DIAG_ALERT_LEAN_CONDITION;
        case FAULT_ID_CATALYST_EFF_DROP: return DIAG_ALERT_EMISSIONS_DEGRADED;
        case FAULT_ID_VOLTAGE_UNSTABLE: return DIAG_ALERT_VOLTAGE_UNSTABLE;
        case FAULT_ID_MODULE_SUPPLY_INSTABILITY: return DIAG_ALERT_ELECTRONICS_INSTABILITY;
        case FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT: return DIAG_ALERT_ABS_INCONSISTENT;
        default: return DIAG_ALERT_NONE;
    }
}

static int find_fault_index(const SimulationState& state, uint8_t fault_id) {
    for (uint8_t i = 0; i < state.active_fault_count; i++) {
        if (state.active_faults[i].fault_id == fault_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint8_t diagnostic_get_probable_root_fault_id(const SimulationState& state) {
    uint8_t best_fault_id = FAULT_ID_NONE;
    int best_score = -1;

    for (uint8_t i = 0; i < state.active_fault_count; i++) {
        const ActiveFault& fault = state.active_faults[i];
        const FaultCatalogEntry* entry = fault_catalog_get(fault.fault_id);
        const uint8_t candidate = entry->is_root_cause ? entry->fault_id : entry->root_fault_id;
        const FaultCatalogEntry* candidate_entry = fault_catalog_get(candidate);
        const int score =
            static_cast<int>(fault.severity) * 40 +
            static_cast<int>(fault.stage) * 12 +
            static_cast<int>(fault.intensity_pct) +
            (candidate_entry->is_root_cause ? 8 : 0);
        if (score > best_score) {
            best_score = score;
            best_fault_id = candidate;
        }
    }

    return best_fault_id;
}

static bool alert_exists(const SimulationState& state, uint8_t alert_type) {
    for (uint8_t i = 0; i < state.alert_count; i++) {
        if (state.alerts[i].type == alert_type) {
            return true;
        }
    }
    return false;
}

static void push_alert(SimulationState& state, uint8_t alert_type, const ActiveFault& fault, uint8_t flags) {
    if (alert_type == DIAG_ALERT_NONE || state.alert_count >= DIAG_MAX_ALERTS || alert_exists(state, alert_type)) {
        return;
    }

    DiagnosticAlert& alert = state.alerts[state.alert_count++];
    alert.type = alert_type;
    alert.severity = fault.severity;
    alert.system = fault.system;
    alert.compartment = fault.compartment;
    alert.fault_id = fault.fault_id;
    alert.flags = flags;
}

const DiagnosticAlert* diagnostic_get_primary_alert(const SimulationState& state) {
    if (state.alert_count == 0) {
        return nullptr;
    }

    const uint8_t probable_root = diagnostic_get_probable_root_fault_id(state);
    int best_index = 0;
    int best_score = -1;

    for (uint8_t i = 0; i < state.alert_count; i++) {
        const DiagnosticAlert& alert = state.alerts[i];
        int score = static_cast<int>(alert.severity) * 100;
        const int fault_index = find_fault_index(state, alert.fault_id);
        if (fault_index >= 0) {
            const ActiveFault& fault = state.active_faults[fault_index];
            score += static_cast<int>(fault.intensity_pct);
            if (fault.stage == FAULT_STAGE_PERSISTENT) {
                score += 25;
            }
            if (fault.fault_id == probable_root || fault_catalog_get(fault.fault_id)->root_fault_id == probable_root) {
                score += 20;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_index = static_cast<int>(i);
        }
    }

    return &state.alerts[best_index];
}

void diagnostic_build_alerts_and_health(SimulationState& state) {
    memset(state.alerts, 0, sizeof(state.alerts));
    state.alert_count = 0;
    state.limp_mode = 0;

    float score = 100.0f;
    const uint8_t probable_root = diagnostic_get_probable_root_fault_id(state);

    for (uint8_t i = 0; i < state.active_fault_count; i++) {
        const ActiveFault& fault = state.active_faults[i];
        const FaultCatalogEntry* entry = fault_catalog_get(fault.fault_id);
        score -= static_cast<float>(entry->base_penalty) *
                 stage_factor(fault.stage) *
                 (static_cast<float>(fault.intensity_pct) / 100.0f);

        uint8_t flags = 0;
        if (entry->is_root_cause || entry->root_fault_id == probable_root) {
            flags |= 0x02;
        }
        push_alert(state, alert_type_for_fault(fault.fault_id), fault, flags);

        if (fault.fault_id == FAULT_ID_COOLING_OVERTEMP &&
            fault.stage == FAULT_STAGE_PERSISTENT &&
            fault.intensity_pct >= 85) {
            state.limp_mode = 1;
        }
    }

    if (score < 5.0f) {
        score = 5.0f;
    }
    if (score > 100.0f) {
        score = 100.0f;
    }
    state.health_score = static_cast<uint8_t>(std::lround(score));

    if (state.health_score <= 20) {
        state.limp_mode = 1;
    }

    if (state.limp_mode && state.alert_count < DIAG_MAX_ALERTS) {
        ActiveFault limp_fault = {};
        limp_fault.fault_id = probable_root;
        limp_fault.system = DIAG_SYS_ENGINE;
        limp_fault.compartment = DIAG_COMP_POWERTRAIN;
        limp_fault.severity = DIAG_SEV_CRITICAL;
        limp_fault.stage = FAULT_STAGE_PERSISTENT;
        limp_fault.intensity_pct = 100;
        push_alert(state, DIAG_ALERT_LIMP_MODE_ACTIVE, limp_fault, 0x01);
    }

    const DiagnosticAlert* primary = diagnostic_get_primary_alert(state);
    if (primary) {
        for (uint8_t i = 0; i < state.alert_count; i++) {
            if (&state.alerts[i] == primary) {
                state.alerts[i].flags |= 0x01;
                break;
            }
        }
    }
}
