#include "mode06_handler.h"

#include "diagnostic_scenarios.h"
#include "fault_catalog.h"

#include <Arduino.h>

namespace {

struct Mode06MonitorSpec {
    uint8_t tid;
    uint8_t cid;
    uint8_t min_limit;
    uint8_t max_limit;
};

constexpr Mode06MonitorSpec MODE06_MONITORS[] = {
    { 0x01, 0x01, 0x00, 0x64 }, // catalyst
    { 0x03, 0x01, 0x00, 0x64 }, // O2 B1S1
    { 0x21, 0x01, 0x00, 0x64 }, // O2 heater B1S1
    { 0x41, 0x01, 0x00, 0x64 }, // misfire cyl 1
    { 0x44, 0x01, 0x00, 0x64 }, // misfire cyl 4
    { 0x81, 0x01, 0x00, 0x64 }, // fuel system bank 1
};

constexpr uint8_t MODE06_PASS_BASE = 0x32;
constexpr uint8_t MODE06_WARN_BASE = 0x68;
constexpr uint8_t MODE06_FAIL_BASE = 0x8C;

static OBDResponse negResp(uint8_t nrc) {
    OBDResponse r;
    r.negative = true;
    r.nrc = nrc;
    return r;
}

static uint8_t fault_intensity_pct(const SimulationState& state, uint8_t fault_id) {
    for (uint8_t i = 0; i < state.active_fault_count; i++) {
        if (state.active_faults[i].fault_id == fault_id) {
            return state.active_faults[i].intensity_pct;
        }
    }
    return 0;
}

static bool scenario_active(const SimulationState& state, DiagnosticScenarioId id) {
    return static_cast<DiagnosticScenarioId>(state.scenario_id) == id;
}

static uint8_t degraded_value(uint8_t intensity_pct, bool fail) {
    const uint8_t base = fail ? MODE06_FAIL_BASE : MODE06_WARN_BASE;
    const uint8_t swing = static_cast<uint8_t>((static_cast<uint16_t>(intensity_pct) * 35u) / 100u);
    return static_cast<uint8_t>(base + swing);
}

static uint8_t monitor_value_for_tid(const SimulationState& state, uint8_t tid) {
    const auto scenario_id = static_cast<DiagnosticScenarioId>(state.scenario_id);

    switch (tid) {
        case 0x01:
            if (scenario_active(state, DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_CATALYST_EFF_DROP), true);
            }
            return MODE06_PASS_BASE;

        case 0x03:
            if (scenario_active(state, DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_AIR_LEAK_IDLE), true);
            }
            if (scenario_active(state, DIAG_SCENARIO_HIGHWAY_CATALYST_EFFICIENCY_DROP)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_CATALYST_EFF_DROP), false);
            }
            return MODE06_PASS_BASE;

        case 0x21:
            return MODE06_PASS_BASE;

        case 0x41:
            if (scenario_active(state, DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_MISFIRE_CYL_1), true);
            }
            return MODE06_PASS_BASE;

        case 0x44:
            if (scenario_active(state, DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_MISFIRE_CYL_4), true);
            }
            return MODE06_PASS_BASE;

        case 0x81:
            if (scenario_active(state, DIAG_SCENARIO_IDLE_LEAN_AIR_LEAK)) {
                return degraded_value(fault_intensity_pct(state, FAULT_ID_AIR_LEAK_IDLE), true);
            }
            if (scenario_id == DIAG_SCENARIO_URBAN_MISFIRE_PROGRESSIVE) {
                const uint8_t intensity = fault_intensity_pct(state, FAULT_ID_MISFIRE_MULTI);
                if (intensity >= 65) {
                    return degraded_value(intensity, true);
                }
                if (intensity >= 35) {
                    return degraded_value(intensity, false);
                }
            }
            return MODE06_PASS_BASE;

        default:
            return MODE06_PASS_BASE;
    }
}

static bool append_monitor_block(OBDResponse& resp, const Mode06MonitorSpec& spec, const SimulationState& state) {
    if (resp.len + 9 > sizeof(resp.data)) {
        return false;
    }

    const uint8_t value = monitor_value_for_tid(state, spec.tid);
    resp.data[resp.len++] = 0x46;
    resp.data[resp.len++] = spec.tid;
    resp.data[resp.len++] = spec.cid;
    resp.data[resp.len++] = 0x00;
    resp.data[resp.len++] = value;
    resp.data[resp.len++] = 0x00;
    resp.data[resp.len++] = spec.min_limit;
    resp.data[resp.len++] = 0x00;
    resp.data[resp.len++] = spec.max_limit;
    return true;
}

static const Mode06MonitorSpec* find_monitor(uint8_t tid) {
    for (size_t i = 0; i < sizeof(MODE06_MONITORS) / sizeof(MODE06_MONITORS[0]); i++) {
        if (MODE06_MONITORS[i].tid == tid) {
            return &MODE06_MONITORS[i];
        }
    }
    return nullptr;
}

} // namespace

OBDResponse Mode06Handler::handle(const OBDRequest& req, const SimulationState& state) {
    OBDResponse resp;

    if (req.pid == 0x00) {
        for (size_t i = 0; i < sizeof(MODE06_MONITORS) / sizeof(MODE06_MONITORS[0]); i++) {
            if (!append_monitor_block(resp, MODE06_MONITORS[i], state)) {
                return negResp(0x72);
            }
        }
        return resp;
    }

    const Mode06MonitorSpec* spec = find_monitor(req.pid);
    if (!spec) {
        return negResp(0x12);
    }

    if (!append_monitor_block(resp, *spec, state)) {
        return negResp(0x72);
    }
    return resp;
}
