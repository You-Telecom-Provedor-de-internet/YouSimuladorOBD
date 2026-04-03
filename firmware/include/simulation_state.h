#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "alert_types.h"

enum SimMode : uint8_t {
    SIM_STATIC = 0,
    SIM_IDLE = 1,
    SIM_URBAN = 2,
    SIM_HIGHWAY = 3,
    SIM_WARMUP = 4,
    SIM_FAULT = 5,
};

constexpr uint8_t SIM_MODE_COUNT = 6;
constexpr uint8_t SIM_MAX_DTCS = 8;
constexpr uint8_t DIAG_MAX_ACTIVE_FAULTS = 6;
constexpr uint8_t DIAG_MAX_ALERTS = 6;

enum OdometerSource : uint8_t {
    ODOMETER_SOURCE_NONE = 0,
    ODOMETER_SOURCE_CLUSTER = 1,
    ODOMETER_SOURCE_BCM = 2,
    ODOMETER_SOURCE_ABS = 3,
    ODOMETER_SOURCE_ECM = 4,
};

struct SimulationState {
    uint16_t rpm = 800;
    uint8_t  speed_kmh = 0;
    int16_t  coolant_temp_c = 90;
    int16_t  intake_temp_c = 30;
    float    maf_gs = 3.0f;
    uint8_t  map_kpa = 35;
    uint8_t  throttle_pct = 15;
    float    ignition_adv = 10.0f;
    uint8_t  engine_load_pct = 25;
    uint8_t  fuel_level_pct = 75;
    float    battery_voltage = 14.2f;
    int16_t  oil_temp_c = 85;
    float    stft_pct = 0.0f;
    float    ltft_pct = 0.0f;

    uint8_t  dtc_count = 0;
    uint16_t dtcs[SIM_MAX_DTCS] = {};

    char vin[18] = "9YSMULMS0T1234567";
    uint8_t active_protocol = 0;
    char profile_id[24] = "";
    SimMode sim_mode = SIM_STATIC;

    uint8_t scenario_id = 0;
    uint8_t health_score = 100;
    uint8_t limp_mode = 0;
    uint8_t active_fault_count = 0;
    uint8_t alert_count = 0;
    uint8_t manual_dtc_count = 0;
    uint16_t manual_dtcs[SIM_MAX_DTCS] = {};
    ActiveFault active_faults[DIAG_MAX_ACTIVE_FAULTS] = {};
    DiagnosticAlert alerts[DIAG_MAX_ALERTS] = {};
    uint32_t odometer_total_km_x10 = 0;
    uint16_t distance_since_clear_km = 0;
    uint16_t distance_mil_on_km = 0;
    uint8_t odometer_source = ODOMETER_SOURCE_CLUSTER;
    uint8_t pid_a6_supported = 0;
};

inline void simulation_clear_effective_dtcs(SimulationState& s) {
    s.dtc_count = 0;
    memset(s.dtcs, 0, sizeof(s.dtcs));
}

inline void simulation_clear_manual_dtcs(SimulationState& s) {
    s.manual_dtc_count = 0;
    memset(s.manual_dtcs, 0, sizeof(s.manual_dtcs));
}

inline bool simulation_has_manual_dtc(const SimulationState& s, uint16_t dtc) {
    for (uint8_t i = 0; i < s.manual_dtc_count; i++) {
        if (s.manual_dtcs[i] == dtc) {
            return true;
        }
    }
    return false;
}

inline bool simulation_add_manual_dtc(SimulationState& s, uint16_t dtc) {
    if (dtc == 0 || simulation_has_manual_dtc(s, dtc) || s.manual_dtc_count >= SIM_MAX_DTCS) {
        return false;
    }
    s.manual_dtcs[s.manual_dtc_count++] = dtc;
    return true;
}

inline bool simulation_remove_manual_dtc(SimulationState& s, uint16_t dtc) {
    for (uint8_t i = 0; i < s.manual_dtc_count; i++) {
        if (s.manual_dtcs[i] == dtc) {
            for (uint8_t j = i; j + 1 < s.manual_dtc_count; j++) {
                s.manual_dtcs[j] = s.manual_dtcs[j + 1];
            }
            s.manual_dtcs[s.manual_dtc_count - 1] = 0;
            s.manual_dtc_count--;
            return true;
        }
    }
    return false;
}

inline void simulation_copy_manual_dtcs_to_effective(SimulationState& s) {
    simulation_clear_effective_dtcs(s);
    s.dtc_count = s.manual_dtc_count;
    for (uint8_t i = 0; i < s.manual_dtc_count; i++) {
        s.dtcs[i] = s.manual_dtcs[i];
    }
}

inline void diagnostic_reset_fields(SimulationState& s) {
    s.scenario_id = 0;
    s.health_score = 100;
    s.limp_mode = 0;
    s.active_fault_count = 0;
    s.alert_count = 0;
    simulation_clear_manual_dtcs(s);
    memset(s.active_faults, 0, sizeof(s.active_faults));
    memset(s.alerts, 0, sizeof(s.alerts));
}

inline void simulation_reset_odometer_fields(SimulationState& s) {
    s.odometer_total_km_x10 = 0;
    s.distance_since_clear_km = 0;
    s.distance_mil_on_km = 0;
    s.odometer_source = ODOMETER_SOURCE_CLUSTER;
    s.pid_a6_supported = 0;
}

inline void simulation_reset_distance_since_clear(SimulationState& s) {
    s.distance_since_clear_km = 0;
}

inline bool simulation_mil_active(const SimulationState& s) {
    return s.dtc_count > 0;
}

static_assert(std::is_trivially_copyable<SimulationState>::value, "SimulationState must stay trivially copyable");
static_assert(sizeof(SimulationState) <= 224, "SimulationState exceeded shared-state budget");

namespace Preset {
    inline void applyOff(SimulationState& s) {
        s.rpm = 0;
        s.speed_kmh = 0;
        s.coolant_temp_c = 20;
        s.intake_temp_c = 20;
        s.maf_gs = 0.0f;
        s.map_kpa = 100;
        s.throttle_pct = 0;
        s.ignition_adv = 0.0f;
        s.engine_load_pct = 0;
        s.fuel_level_pct = 75;
        s.battery_voltage = 12.5f;
        s.oil_temp_c = 20;
        s.stft_pct = 0.0f;
        s.ltft_pct = 0.0f;
        diagnostic_reset_fields(s);
        simulation_clear_effective_dtcs(s);
        s.sim_mode = SIM_STATIC;
    }

    inline void applyIdle(SimulationState& s) {
        s.rpm = 800;
        s.speed_kmh = 0;
        s.coolant_temp_c = 90;
        s.intake_temp_c = 30;
        s.maf_gs = 3.0f;
        s.map_kpa = 35;
        s.throttle_pct = 15;
        s.ignition_adv = 10.0f;
        s.engine_load_pct = 20;
        s.fuel_level_pct = 75;
        s.battery_voltage = 14.2f;
        s.oil_temp_c = 85;
        s.stft_pct = 0.0f;
        s.ltft_pct = 0.0f;
        diagnostic_reset_fields(s);
        simulation_clear_effective_dtcs(s);
        s.sim_mode = SIM_STATIC;
    }

    inline void applyCruise(SimulationState& s) {
        s.rpm = 2000;
        s.speed_kmh = 60;
        s.coolant_temp_c = 92;
        s.intake_temp_c = 35;
        s.maf_gs = 12.0f;
        s.map_kpa = 55;
        s.throttle_pct = 35;
        s.ignition_adv = 15.0f;
        s.engine_load_pct = 45;
        s.fuel_level_pct = 60;
        s.battery_voltage = 14.0f;
        s.oil_temp_c = 90;
        s.stft_pct = -1.6f;
        s.ltft_pct = 0.0f;
        diagnostic_reset_fields(s);
        simulation_clear_effective_dtcs(s);
        s.sim_mode = SIM_STATIC;
    }

    inline void applyFullThrottle(SimulationState& s) {
        s.rpm = 5000;
        s.speed_kmh = 120;
        s.coolant_temp_c = 98;
        s.intake_temp_c = 45;
        s.maf_gs = 55.0f;
        s.map_kpa = 95;
        s.throttle_pct = 90;
        s.ignition_adv = 20.0f;
        s.engine_load_pct = 85;
        s.fuel_level_pct = 50;
        s.battery_voltage = 13.8f;
        s.oil_temp_c = 100;
        s.stft_pct = 4.7f;
        s.ltft_pct = 1.6f;
        diagnostic_reset_fields(s);
        simulation_clear_effective_dtcs(s);
        s.sim_mode = SIM_STATIC;
    }

    inline void applyOverheat(SimulationState& s) {
        applyIdle(s);
        s.coolant_temp_c = 115;
        s.oil_temp_c = 118;
        s.battery_voltage = 13.5f;
        simulation_add_manual_dtc(s, 0x0217);
        simulation_copy_manual_dtcs_to_effective(s);
    }

    inline void applyCatalystFail(SimulationState& s) {
        applyCruise(s);
        s.stft_pct = 12.5f;
        s.ltft_pct = 10.2f;
        simulation_add_manual_dtc(s, 0x0420);
        simulation_copy_manual_dtcs_to_effective(s);
    }
}
