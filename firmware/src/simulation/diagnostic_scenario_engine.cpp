#include "diagnostic_scenario_engine.h"
#include "dynamic_engine.h"

#include <Arduino.h>
#include <cmath>
#include <cstring>

struct FaultRuntime {
    uint8_t fault_id = 0;
    uint8_t stage = FAULT_STAGE_NONE;
    uint8_t intensity_pct = 0;
    uint8_t dtc_latched = 0;
    uint8_t newly_latched = 0;
    uint8_t clear_cooldown_ticks = 0;
    uint16_t elapsed_ticks = 0;
    uint16_t qualify_ticks = 0;
    uint16_t recovery_ticks = 0;
};

struct DiagnosticRuntime {
    DiagnosticScenarioId active_scenario = DIAG_SCENARIO_NONE;
    uint32_t global_tick = 0;
    FaultRuntime faults[DIAG_SCENARIO_MAX_FAULTS] = {};
    DiagnosticFreezeFrame freeze_frame = {};
};

static DiagnosticRuntime s_diag_runtime;

static const ScenarioDefinition SCENARIOS[] = {
    {},
    {
        DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE,
        "urban_misfire_progressive",
        "Misfire urbano",
        "Misfire com perda de torque e trim positivo.",
        SIM_URBAN,
        sim_mode_mask(SIM_URBAN),
        3,
        {
            { FAULT_ID_MISFIRE_MULTI, 20, 40, 60, 90, 180, 20, 92 },
            { FAULT_ID_MISFIRE_CYL_1, 45, 25, 45, 90, 180, 15, 78 },
            { FAULT_ID_MISFIRE_CYL_4, 70, 25, 45, 90, 180, 15, 74 },
        }
    },
    {
        DIAG_SCENARIO_URBAN_OVERHEAT_FAN_DEGRADED,
        "urban_overheat_fan_degraded",
        "Arrefecimento degradado",
        "Aquece em baixa e alivia em rodovia.",
        SIM_URBAN,
        static_cast<uint8_t>(sim_mode_mask(SIM_URBAN) | sim_mode_mask(SIM_HIGHWAY)),
        2,
        {
            { FAULT_ID_FAN_DEGRADED, 10, 50, 70, 100, 220, 20, 80 },
            { FAULT_ID_COOLING_OVERTEMP, 120, 40, 60, 80, 220, 20, 100 },
        }
    },
    {
        DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK,
        "idle_lean_air_leak",
        "Falso ar em marcha lenta",
        "Idle instavel com MAP alto e trims positivos.",
        SIM_IDLE,
        static_cast<uint8_t>(sim_mode_mask(SIM_IDLE) | sim_mode_mask(SIM_URBAN)),
        1,
        {
            { FAULT_ID_AIR_LEAK_IDLE, 10, 40, 80, 100, 220, 15, 88 },
        }
    },
    {
        DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP,
        "highway_catalyst_efficiency_drop",
        "Catalisador degradado",
        "Emissoes degradadas com pouca perda.",
        SIM_HIGHWAY,
        sim_mode_mask(SIM_HIGHWAY),
        1,
        {
            { FAULT_ID_CATALYST_EFF_DROP, 40, 80, 120, 120, 260, 30, 72 },
        }
    },
    {
        DIAG_SCENARIO_UNSTABLE_VOLTAGE_MULTI_MODULE,
        "unstable_voltage_multi_module",
        "Tensao instavel",
        "Carga instavel afetando leituras e modulos.",
        SIM_IDLE,
        static_cast<uint8_t>(sim_mode_mask(SIM_STATIC) | sim_mode_mask(SIM_IDLE) | sim_mode_mask(SIM_URBAN) |
                             sim_mode_mask(SIM_HIGHWAY) | sim_mode_mask(SIM_WARMUP) | sim_mode_mask(SIM_FAULT)),
        2,
        {
            { FAULT_ID_VOLTAGE_UNSTABLE, 5, 30, 70, 90, 240, 25, 94 },
            { FAULT_ID_MODULE_SUPPLY_INSTABILITY, 25, 30, 60, 90, 200, 20, 72 },
        }
    },
    {
        DIAG_SCENARIO_WHEEL_SPEED_ABS_INCONSISTENT,
        "wheel_speed_abs_inconsistent",
        "Inconsistencia ABS",
        "Falha interna ABS sem DTC OBD de emissao.",
        SIM_URBAN,
        static_cast<uint8_t>(sim_mode_mask(SIM_URBAN) | sim_mode_mask(SIM_HIGHWAY)),
        1,
        {
            { FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT, 20, 35, 60, 100, 220, 20, 70 },
        }
    },
};

static float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static uint8_t clamp_u8(int value, int lo, int hi) {
    if (value < lo) return static_cast<uint8_t>(lo);
    if (value > hi) return static_cast<uint8_t>(hi);
    return static_cast<uint8_t>(value);
}

static uint16_t clamp_u16(int value, int lo, int hi) {
    if (value < lo) return static_cast<uint16_t>(lo);
    if (value > hi) return static_cast<uint16_t>(hi);
    return static_cast<uint16_t>(value);
}

static const ScenarioDefinition* current_definition(const SimulationState& state) {
    return diagnostic_scenario_get(static_cast<DiagnosticScenarioId>(state.scenario_id));
}

const ScenarioDefinition* diagnostic_scenario_get(DiagnosticScenarioId id) {
    for (size_t i = 0; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); i++) {
        if (SCENARIOS[i].id == id) {
            return &SCENARIOS[i];
        }
    }
    return &SCENARIOS[0];
}

const ScenarioDefinition* diagnostic_scenario_all(size_t& count) {
    count = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);
    return SCENARIOS;
}

DiagnosticScenarioId diagnostic_scenario_from_slug(const char* slug) {
    if (!slug || !slug[0]) {
        return DIAG_SCENARIO_NONE;
    }
    for (size_t i = 1; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); i++) {
        if (strcmp(SCENARIOS[i].slug, slug) == 0) {
            return SCENARIOS[i].id;
        }
    }
    return DIAG_SCENARIO_NONE;
}

const char* diagnostic_scenario_slug(DiagnosticScenarioId id) {
    return diagnostic_scenario_get(id)->slug;
}

const char* diagnostic_scenario_label(DiagnosticScenarioId id) {
    return diagnostic_scenario_get(id)->label;
}

bool diagnostic_scenario_allows_mode(DiagnosticScenarioId id, SimMode mode) {
    const ScenarioDefinition* definition = diagnostic_scenario_get(id);
    return definition->id == DIAG_SCENARIO_NONE ||
           ((definition->allowed_mode_mask & sim_mode_mask(mode)) != 0);
}

void diagnostic_reset_runtime() {
    memset(&s_diag_runtime, 0, sizeof(s_diag_runtime));
}

void diagnostic_engine_init() {
    diagnostic_reset_runtime();
}

static void restore_manual_dtcs(SimulationState& state, uint16_t* manual_dtcs, uint8_t count) {
    simulation_clear_manual_dtcs(state);
    for (uint8_t i = 0; i < count && i < SIM_MAX_DTCS; i++) {
        simulation_add_manual_dtc(state, manual_dtcs[i]);
    }
}

void diagnostic_engine_clear_scenario(SimulationState& state, bool preserve_manual_dtcs) {
    uint16_t saved_manual_dtcs[SIM_MAX_DTCS] = {};
    const uint8_t saved_manual_count = preserve_manual_dtcs ? state.manual_dtc_count : 0;
    if (preserve_manual_dtcs) {
        memcpy(saved_manual_dtcs, state.manual_dtcs, sizeof(saved_manual_dtcs));
    }

    diagnostic_reset_runtime();
    diagnostic_reset_fields(state);
    state.scenario_id = DIAG_SCENARIO_NONE;

    if (preserve_manual_dtcs) {
        restore_manual_dtcs(state, saved_manual_dtcs, saved_manual_count);
    }

    simulation_copy_manual_dtcs_to_effective(state);
}

void diagnostic_engine_set_scenario(SimulationState& state, DiagnosticScenarioId scenario_id) {
    if (scenario_id == DIAG_SCENARIO_NONE) {
        diagnostic_engine_clear_scenario(state, true);
        return;
    }

    uint16_t saved_manual_dtcs[SIM_MAX_DTCS] = {};
    const uint8_t saved_manual_count = state.manual_dtc_count;
    memcpy(saved_manual_dtcs, state.manual_dtcs, sizeof(saved_manual_dtcs));

    diagnostic_reset_runtime();
    diagnostic_reset_fields(state);
    restore_manual_dtcs(state, saved_manual_dtcs, saved_manual_count);
    state.scenario_id = static_cast<uint8_t>(scenario_id);

    const ScenarioDefinition* definition = diagnostic_scenario_get(scenario_id);
    if (!diagnostic_scenario_allows_mode(scenario_id, state.sim_mode)) {
        state.sim_mode = definition->default_mode;
    }

    diagnostic_engine_rebuild_effective_dtcs(state);
}

static FaultRuntime* runtime_for_fault(uint8_t fault_id) {
    for (uint8_t i = 0; i < DIAG_SCENARIO_MAX_FAULTS; i++) {
        if (s_diag_runtime.faults[i].fault_id == fault_id) {
            return &s_diag_runtime.faults[i];
        }
    }
    return nullptr;
}

static const FaultRuntime* runtime_for_fault(uint8_t fault_id, const ScenarioDefinition&) {
    return runtime_for_fault(fault_id);
}

static bool relief_condition(DiagnosticScenarioId scenario_id, uint8_t fault_id, const SimulationState& state) {
    switch (scenario_id) {
        case DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE:
            return state.speed_kmh < 18 && state.throttle_pct < 14;
        case DIAG_SCENARIO_URBAN_OVERHEAT_FAN_DEGRADED:
            return state.speed_kmh > 70;
        case DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK:
            return state.speed_kmh > 20 || state.throttle_pct > 18;
        case DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP:
            return state.speed_kmh < 75 || state.throttle_pct > 40;
        case DIAG_SCENARIO_UNSTABLE_VOLTAGE_MULTI_MODULE:
            return state.rpm > 1800 && state.speed_kmh > 30 && fault_id == FAULT_ID_VOLTAGE_UNSTABLE;
        case DIAG_SCENARIO_WHEEL_SPEED_ABS_INCONSISTENT:
            return state.speed_kmh < 15;
        default:
            return false;
    }
}

static uint8_t compute_intensity(const ScenarioFaultSpec& spec, const FaultRuntime& runtime) {
    if (runtime.stage == FAULT_STAGE_NONE) {
        return 0;
    }

    const uint16_t age = runtime.elapsed_ticks > spec.start_delay_ticks
        ? runtime.elapsed_ticks - spec.start_delay_ticks
        : 0;
    const float max_intensity = static_cast<float>(spec.max_intensity_pct);

    if (runtime.stage == FAULT_STAGE_ONSET && spec.onset_ticks > 0) {
        const float t = clampf(static_cast<float>(age) / static_cast<float>(spec.onset_ticks), 0.0f, 1.0f);
        return static_cast<uint8_t>(std::lround(15.0f + (max_intensity * 0.35f - 15.0f) * t));
    }

    if (runtime.stage == FAULT_STAGE_AGGRAVATING) {
        const uint16_t start = spec.onset_ticks;
        const uint16_t dur = spec.aggravating_ticks == 0 ? 1 : spec.aggravating_ticks;
        const float t = clampf(static_cast<float>(age > start ? age - start : 0) / static_cast<float>(dur), 0.0f, 1.0f);
        return static_cast<uint8_t>(std::lround(max_intensity * (0.35f + 0.35f * t)));
    }

    if (runtime.stage == FAULT_STAGE_INTERMITTENT) {
        const uint32_t cycle = (s_diag_runtime.global_tick / 8u) % 4u;
        const float multiplier = (cycle == 0u || cycle == 2u) ? 0.78f : 0.52f;
        return static_cast<uint8_t>(std::lround(max_intensity * multiplier));
    }

    if (runtime.stage == FAULT_STAGE_PERSISTENT) {
        return static_cast<uint8_t>(std::lround(max_intensity * 0.95f));
    }

    return static_cast<uint8_t>(std::lround(max_intensity * 0.45f));
}

static uint8_t effective_severity(const FaultCatalogEntry& entry, uint8_t stage, uint8_t intensity_pct) {
    int severity = static_cast<int>(entry.default_severity);
    if (stage == FAULT_STAGE_ONSET || stage == FAULT_STAGE_RECOVERING) {
        severity -= 1;
    }
    if (intensity_pct >= 88) {
        severity += 1;
    }
    if (severity < DIAG_SEV_INFO) severity = DIAG_SEV_INFO;
    if (severity > DIAG_SEV_CRITICAL) severity = DIAG_SEV_CRITICAL;
    return static_cast<uint8_t>(severity);
}

static uint16_t qualification_ticks(uint8_t stage) {
    return stage >= FAULT_STAGE_PERSISTENT ? 10 : 18;
}

static void update_runtime(FaultRuntime& runtime, const ScenarioFaultSpec& spec,
                           DiagnosticScenarioId scenario_id, const SimulationState& state) {
    runtime.fault_id = spec.fault_id;
    runtime.elapsed_ticks++;
    runtime.newly_latched = 0;

    if (runtime.clear_cooldown_ticks > 0) {
        runtime.clear_cooldown_ticks--;
    }

    if (runtime.elapsed_ticks <= spec.start_delay_ticks) {
        runtime.stage = FAULT_STAGE_NONE;
        runtime.intensity_pct = 0;
        runtime.qualify_ticks = 0;
        runtime.recovery_ticks = 0;
        return;
    }

    const uint16_t age = runtime.elapsed_ticks - spec.start_delay_ticks;
    const bool relieving = relief_condition(scenario_id, spec.fault_id, state);

    if (relieving) {
        runtime.recovery_ticks++;
    } else {
        runtime.recovery_ticks = 0;
    }

    if (relieving && runtime.recovery_ticks >= (spec.recovering_ticks == 0 ? 1 : spec.recovering_ticks)) {
        runtime.stage = FAULT_STAGE_RECOVERING;
    } else if (age <= spec.onset_ticks) {
        runtime.stage = FAULT_STAGE_ONSET;
    } else if (age <= static_cast<uint32_t>(spec.onset_ticks) + spec.aggravating_ticks) {
        runtime.stage = FAULT_STAGE_AGGRAVATING;
    } else if (age <= static_cast<uint32_t>(spec.onset_ticks) + spec.aggravating_ticks + spec.intermittent_ticks) {
        runtime.stage = FAULT_STAGE_INTERMITTENT;
    } else {
        runtime.stage = FAULT_STAGE_PERSISTENT;
    }

    runtime.intensity_pct = compute_intensity(spec, runtime);

    const FaultCatalogEntry* entry = fault_catalog_get(spec.fault_id);
    if (entry->emit_obd && runtime.stage >= entry->qualify_stage && runtime.clear_cooldown_ticks == 0) {
        runtime.qualify_ticks++;
        if (!runtime.dtc_latched && runtime.qualify_ticks >= qualification_ticks(runtime.stage)) {
            runtime.dtc_latched = 1;
            runtime.newly_latched = 1;
        }
    } else {
        runtime.qualify_ticks = 0;
    }
}

static void reset_active_faults(SimulationState& state) {
    state.active_fault_count = 0;
    memset(state.active_faults, 0, sizeof(state.active_faults));
}

static void push_active_fault(SimulationState& state, const FaultRuntime& runtime) {
    if (runtime.stage == FAULT_STAGE_NONE || runtime.intensity_pct == 0 || state.active_fault_count >= DIAG_MAX_ACTIVE_FAULTS) {
        return;
    }

    const FaultCatalogEntry* entry = fault_catalog_get(runtime.fault_id);
    ActiveFault& fault = state.active_faults[state.active_fault_count++];
    fault.dtc = entry->dtc;
    fault.fault_id = entry->fault_id;
    fault.system = static_cast<uint8_t>(entry->system);
    fault.compartment = static_cast<uint8_t>(entry->compartment);
    fault.severity = effective_severity(*entry, runtime.stage, runtime.intensity_pct);
    fault.stage = runtime.stage;
    fault.intensity_pct = runtime.intensity_pct;
}

static float fault_intensity(uint8_t fault_id) {
    const FaultRuntime* runtime = runtime_for_fault(fault_id);
    return runtime ? static_cast<float>(runtime->intensity_pct) / 100.0f : 0.0f;
}

static void apply_misfire_effects(SimulationState& state) {
    const float root = fault_intensity(FAULT_ID_MISFIRE_MULTI);
    const float cyl_1 = fault_intensity(FAULT_ID_MISFIRE_CYL_1);
    const float cyl_4 = fault_intensity(FAULT_ID_MISFIRE_CYL_4);
    const float combined = clampf(root + 0.3f * (cyl_1 + cyl_4), 0.0f, 1.45f);
    const float load_factor = 0.45f + static_cast<float>(state.throttle_pct) / 100.0f;
    const float wave = sinf(static_cast<float>(s_diag_runtime.global_tick) * 0.65f) * 90.0f;
    const float stumble = ((s_diag_runtime.global_tick / 6u) % 5u == 0u) ? -140.0f : 25.0f;

    state.rpm = clamp_u16(static_cast<int>(state.rpm + (wave + stumble) * combined * load_factor), 550, 6500);
    state.maf_gs = clampf(state.maf_gs * (1.0f - 0.14f * combined), 0.0f, 655.0f);
    state.engine_load_pct = clamp_u8(static_cast<int>(state.engine_load_pct + 10.0f * combined), 0, 100);
    state.stft_pct = clampf(state.stft_pct + 15.0f * combined + 3.5f * (cyl_1 + cyl_4), -30.0f, 30.0f);
    state.ltft_pct = clampf(state.ltft_pct + 8.5f * combined, -30.0f, 30.0f);
    state.ignition_adv = clampf(state.ignition_adv - 4.0f * combined, -20.0f, 40.0f);
    if (combined > 0.70f && state.speed_kmh > 20) {
        state.speed_kmh = clamp_u8(static_cast<int>(state.speed_kmh) - 1, 0, 255);
    }
}

static void apply_overheat_effects(SimulationState& state) {
    const float fan = fault_intensity(FAULT_ID_FAN_DEGRADED);
    const float overtemp = fault_intensity(FAULT_ID_COOLING_OVERTEMP);
    const float cooling = clampf(fan + overtemp, 0.0f, 1.5f);
    const float relief = clampf((static_cast<float>(state.speed_kmh) - 65.0f) / 35.0f, 0.0f, 1.0f);
    const float heat_load = (1.0f - relief) * cooling;

    state.coolant_temp_c = static_cast<int16_t>(clampf(static_cast<float>(state.coolant_temp_c) + (1.6f + 3.2f * overtemp) * heat_load, -40.0f, 215.0f));
    state.oil_temp_c = static_cast<int16_t>(clampf(static_cast<float>(state.oil_temp_c) + (1.2f + 2.7f * overtemp) * heat_load, -40.0f, 210.0f));
    state.battery_voltage = clampf(state.battery_voltage - 0.35f * fan, 11.8f, 16.0f);

    if (state.limp_mode || overtemp > 0.85f) {
        state.throttle_pct = clamp_u8(state.throttle_pct, 0, 28);
        state.engine_load_pct = clamp_u8(state.engine_load_pct, 0, 42);
        state.rpm = clamp_u16(state.rpm, 0, 2800);
    }
}

static void apply_air_leak_effects(SimulationState& state) {
    const float leak = fault_intensity(FAULT_ID_AIR_LEAK_IDLE);
    const float idle_bias = state.speed_kmh == 0 ? 1.0f : 0.45f;
    const float hunt = sinf(static_cast<float>(s_diag_runtime.global_tick) * 0.50f) * 70.0f;
    const float dip = ((s_diag_runtime.global_tick / 9u) % 4u == 1u) ? -55.0f : 20.0f;

    state.rpm = clamp_u16(static_cast<int>(state.rpm + (hunt + dip) * leak * idle_bias), 650, 2200);
    if (state.throttle_pct < 20) {
        state.map_kpa = clamp_u8(static_cast<int>(state.map_kpa + 5.0f + 11.0f * leak), 0, 255);
    }
    state.maf_gs = clampf(state.maf_gs + 0.5f + 1.4f * leak, 0.0f, 655.0f);
    state.stft_pct = clampf(state.stft_pct + 13.0f + 14.0f * leak, -30.0f, 30.0f);
    state.ltft_pct = clampf(state.ltft_pct + 5.5f + 9.5f * leak, -30.0f, 30.0f);
    state.ignition_adv = clampf(state.ignition_adv - 2.0f * leak, -20.0f, 40.0f);
}

static void apply_catalyst_effects(SimulationState& state) {
    const float cat = fault_intensity(FAULT_ID_CATALYST_EFF_DROP);
    state.stft_pct = clampf(state.stft_pct + 2.5f + 4.5f * cat, -30.0f, 30.0f);
    state.ltft_pct = clampf(state.ltft_pct + 3.0f + 5.0f * cat, -30.0f, 30.0f);
    state.coolant_temp_c = static_cast<int16_t>(clampf(static_cast<float>(state.coolant_temp_c) + 0.4f + 1.2f * cat, -40.0f, 215.0f));
    state.oil_temp_c = static_cast<int16_t>(clampf(static_cast<float>(state.oil_temp_c) + 0.6f + 1.5f * cat, -40.0f, 210.0f));
}

static void apply_voltage_effects(SimulationState& state) {
    const float voltage = fault_intensity(FAULT_ID_VOLTAGE_UNSTABLE);
    const float modules = fault_intensity(FAULT_ID_MODULE_SUPPLY_INSTABILITY);
    const float wave = sinf(static_cast<float>(s_diag_runtime.global_tick) * 0.35f) * 0.95f +
                       sinf(static_cast<float>(s_diag_runtime.global_tick) * 0.91f) * 0.30f;
    const float base_voltage = 13.8f - 1.45f * voltage;
    const float jitter = sinf(static_cast<float>(s_diag_runtime.global_tick + 11u) * 0.42f) * 0.75f;

    state.battery_voltage = clampf(base_voltage + wave * (0.35f + 0.85f * voltage), 11.2f, 14.8f);
    state.map_kpa = clamp_u8(static_cast<int>(state.map_kpa + jitter * 2.4f * modules), 0, 255);
    state.intake_temp_c = static_cast<int16_t>(clampf(state.intake_temp_c + jitter * 1.1f * modules, -40.0f, 80.0f));
    state.coolant_temp_c = static_cast<int16_t>(clampf(state.coolant_temp_c + jitter * 0.5f * modules, -40.0f, 215.0f));
    state.maf_gs = clampf(state.maf_gs + jitter * 0.45f * modules, 0.0f, 655.0f);
    state.rpm = clamp_u16(static_cast<int>(state.rpm + wave * 50.0f * voltage), 0, 16000);
}

static void apply_scenario_effects(DiagnosticScenarioId scenario_id, SimulationState& state) {
    switch (scenario_id) {
        case DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE:
            apply_misfire_effects(state);
            break;
        case DIAG_SCENARIO_URBAN_OVERHEAT_FAN_DEGRADED:
            apply_overheat_effects(state);
            break;
        case DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK:
            apply_air_leak_effects(state);
            break;
        case DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP:
            apply_catalyst_effects(state);
            break;
        case DIAG_SCENARIO_UNSTABLE_VOLTAGE_MULTI_MODULE:
            apply_voltage_effects(state);
            break;
        case DIAG_SCENARIO_WHEEL_SPEED_ABS_INCONSISTENT:
        default:
            break;
    }
}

static void capture_freeze_frame(const SimulationState& state, uint8_t fault_id, uint16_t dtc) {
    if (s_diag_runtime.freeze_frame.valid) {
        return;
    }

    s_diag_runtime.freeze_frame.valid = 1;
    s_diag_runtime.freeze_frame.fault_id = fault_id;
    s_diag_runtime.freeze_frame.health_score = state.health_score;
    s_diag_runtime.freeze_frame.speed_kmh = state.speed_kmh;
    s_diag_runtime.freeze_frame.throttle_pct = state.throttle_pct;
    s_diag_runtime.freeze_frame.dtc = dtc;
    s_diag_runtime.freeze_frame.rpm = state.rpm;
    s_diag_runtime.freeze_frame.coolant_temp_c = state.coolant_temp_c;
    s_diag_runtime.freeze_frame.intake_temp_c = state.intake_temp_c;
    s_diag_runtime.freeze_frame.oil_temp_c = state.oil_temp_c;
    s_diag_runtime.freeze_frame.maf_gs = state.maf_gs;
    s_diag_runtime.freeze_frame.map_kpa = state.map_kpa;
    s_diag_runtime.freeze_frame.battery_voltage = state.battery_voltage;
    s_diag_runtime.freeze_frame.stft_pct = state.stft_pct;
    s_diag_runtime.freeze_frame.ltft_pct = state.ltft_pct;
}

struct DtcCandidate {
    uint16_t dtc = 0;
    uint8_t severity = 0;
    uint8_t order = 0;
};

void diagnostic_engine_rebuild_effective_dtcs(SimulationState& state) {
    DtcCandidate candidates[SIM_MAX_DTCS + DIAG_MAX_ACTIVE_FAULTS] = {};
    uint8_t candidate_count = 0;
    uint8_t order = 0;

    for (uint8_t i = 0; i < DIAG_SCENARIO_MAX_FAULTS && candidate_count < (SIM_MAX_DTCS + DIAG_MAX_ACTIVE_FAULTS); i++) {
        const FaultRuntime& runtime = s_diag_runtime.faults[i];
        const FaultCatalogEntry* entry = fault_catalog_get(runtime.fault_id);
        if (!runtime.dtc_latched || !entry->emit_obd || entry->dtc == 0) {
            continue;
        }

        bool duplicate = false;
        for (uint8_t j = 0; j < candidate_count; j++) {
            if (candidates[j].dtc == entry->dtc) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            candidates[candidate_count++] = { entry->dtc, static_cast<uint8_t>(entry->default_severity), order++ };
        }
    }

    for (uint8_t i = 0; i < state.manual_dtc_count && candidate_count < (SIM_MAX_DTCS + DIAG_MAX_ACTIVE_FAULTS); i++) {
        const uint16_t dtc = state.manual_dtcs[i];
        bool duplicate = false;
        for (uint8_t j = 0; j < candidate_count; j++) {
            if (candidates[j].dtc == dtc) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            candidates[candidate_count++] = { dtc, DIAG_SEV_WARNING, order++ };
        }
    }

    for (uint8_t i = 0; i < candidate_count; i++) {
        for (uint8_t j = i + 1; j < candidate_count; j++) {
            if (candidates[j].severity > candidates[i].severity ||
                (candidates[j].severity == candidates[i].severity && candidates[j].order < candidates[i].order)) {
                const DtcCandidate tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }

    simulation_clear_effective_dtcs(state);
    for (uint8_t i = 0; i < candidate_count && i < SIM_MAX_DTCS; i++) {
        state.dtcs[state.dtc_count++] = candidates[i].dtc;
    }
}

bool diagnostic_add_manual_dtc(SimulationState& state, uint16_t dtc) {
    const bool changed = simulation_add_manual_dtc(state, dtc);
    diagnostic_engine_rebuild_effective_dtcs(state);
    return changed;
}

bool diagnostic_remove_manual_dtc(SimulationState& state, uint16_t dtc) {
    const bool changed = simulation_remove_manual_dtc(state, dtc);
    diagnostic_engine_rebuild_effective_dtcs(state);
    return changed;
}

void diagnostic_engine_handle_mode04_clear(SimulationState& state) {
    simulation_clear_manual_dtcs(state);
    simulation_reset_distance_since_clear(state);
    state.distance_mil_on_km = 0;
    dynamic_reset_odometer_service_counters(state);
    for (uint8_t i = 0; i < DIAG_SCENARIO_MAX_FAULTS; i++) {
        s_diag_runtime.faults[i].dtc_latched = 0;
        s_diag_runtime.faults[i].newly_latched = 0;
        s_diag_runtime.faults[i].qualify_ticks = 0;
        s_diag_runtime.faults[i].clear_cooldown_ticks = 20;
    }
    memset(&s_diag_runtime.freeze_frame, 0, sizeof(s_diag_runtime.freeze_frame));
    diagnostic_engine_rebuild_effective_dtcs(state);
}

bool diagnostic_get_freeze_frame(DiagnosticFreezeFrame& out_frame) {
    if (!s_diag_runtime.freeze_frame.valid) {
        return false;
    }
    out_frame = s_diag_runtime.freeze_frame;
    return true;
}

static void clear_overlay_preserve_manual(SimulationState& state) {
    uint16_t saved_manual_dtcs[SIM_MAX_DTCS] = {};
    const uint8_t saved_manual_count = state.manual_dtc_count;
    memcpy(saved_manual_dtcs, state.manual_dtcs, sizeof(saved_manual_dtcs));

    diagnostic_reset_fields(state);
    restore_manual_dtcs(state, saved_manual_dtcs, saved_manual_count);
    simulation_copy_manual_dtcs_to_effective(state);
}

void diagnostic_engine_step(SimulationState& state, float) {
    s_diag_runtime.global_tick++;

    DiagnosticScenarioId scenario_id = static_cast<DiagnosticScenarioId>(state.scenario_id);
    const ScenarioDefinition* definition = diagnostic_scenario_get(scenario_id);

    if (definition->id == DIAG_SCENARIO_NONE) {
        if (s_diag_runtime.active_scenario != DIAG_SCENARIO_NONE || state.active_fault_count || state.alert_count || state.limp_mode || state.health_score != 100) {
            diagnostic_reset_runtime();
            clear_overlay_preserve_manual(state);
        } else {
            diagnostic_engine_rebuild_effective_dtcs(state);
        }
        return;
    }

    if (s_diag_runtime.active_scenario != scenario_id) {
        diagnostic_reset_runtime();
        s_diag_runtime.active_scenario = scenario_id;
    }

    if (!diagnostic_scenario_allows_mode(scenario_id, state.sim_mode)) {
        state.sim_mode = definition->default_mode;
    }

    reset_active_faults(state);
    uint8_t freeze_fault_id = FAULT_ID_NONE;
    uint16_t freeze_dtc = 0;

    for (uint8_t i = 0; i < definition->fault_count && i < DIAG_SCENARIO_MAX_FAULTS; i++) {
        FaultRuntime& runtime = s_diag_runtime.faults[i];
        update_runtime(runtime, definition->faults[i], scenario_id, state);
        push_active_fault(state, runtime);
        if (runtime.newly_latched && freeze_fault_id == FAULT_ID_NONE) {
            const FaultCatalogEntry* entry = fault_catalog_get(runtime.fault_id);
            freeze_fault_id = entry->fault_id;
            freeze_dtc = entry->dtc;
        }
    }

    apply_scenario_effects(scenario_id, state);
    diagnostic_build_alerts_and_health(state);

    if (freeze_fault_id != FAULT_ID_NONE) {
        capture_freeze_frame(state, freeze_fault_id, freeze_dtc);
    }

    diagnostic_engine_rebuild_effective_dtcs(state);
}
