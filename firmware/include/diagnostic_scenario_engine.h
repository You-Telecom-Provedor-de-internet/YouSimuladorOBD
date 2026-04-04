#pragma once

#include "diagnostic_scenarios.h"
#include "fault_catalog.h"

constexpr uint8_t DIAG_FREEZE_FRAME_HISTORY = 4;

enum DiagnosticFreezeFrameSource : uint8_t {
    DIAG_FREEZE_SOURCE_NONE = 0,
    DIAG_FREEZE_SOURCE_SCENARIO = 1,
    DIAG_FREEZE_SOURCE_MANUAL = 2,
    DIAG_FREEZE_SOURCE_EFFECTIVE = 3,
};

struct DiagnosticFreezeFrame {
    uint8_t valid = 0;
    uint8_t fault_id = 0;
    uint8_t scenario_id = 0;
    uint8_t source = DIAG_FREEZE_SOURCE_NONE;
    uint8_t health_score = 100;
    uint8_t speed_kmh = 0;
    uint8_t throttle_pct = 0;
    uint8_t engine_load_pct = 0;
    uint8_t fuel_level_pct = 0;
    uint16_t dtc = 0;
    uint16_t sequence = 0;
    uint16_t rpm = 0;
    int16_t coolant_temp_c = 0;
    int16_t intake_temp_c = 0;
    int16_t oil_temp_c = 0;
    float maf_gs = 0.0f;
    uint8_t map_kpa = 0;
    float ignition_adv = 0.0f;
    float battery_voltage = 0.0f;
    float stft_pct = 0.0f;
    float ltft_pct = 0.0f;
};

void diagnostic_engine_init();
void diagnostic_reset_runtime();
void diagnostic_engine_step(SimulationState& state, float dt_s);
void diagnostic_engine_set_scenario(SimulationState& state, DiagnosticScenarioId scenario_id);
void diagnostic_engine_clear_scenario(SimulationState& state, bool preserve_manual_dtcs = true);
void diagnostic_engine_handle_mode04_clear(SimulationState& state);
void diagnostic_engine_rebuild_effective_dtcs(SimulationState& state);

bool diagnostic_add_manual_dtc(SimulationState& state, uint16_t dtc);
bool diagnostic_remove_manual_dtc(SimulationState& state, uint16_t dtc);

const DiagnosticAlert* diagnostic_get_primary_alert(const SimulationState& state);
uint8_t diagnostic_get_probable_root_fault_id(const SimulationState& state);
bool diagnostic_get_freeze_frame(DiagnosticFreezeFrame& out_frame);
uint8_t diagnostic_get_freeze_frames(DiagnosticFreezeFrame* out_frames, uint8_t capacity);

void diagnostic_build_alerts_and_health(SimulationState& state);
