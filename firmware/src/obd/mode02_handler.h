#pragma once
#include "obd_types.h"
#include "simulation_state.h"
#include "diagnostic_scenario_engine.h"

class Mode02Handler {
public:
    OBDResponse handle(const OBDRequest& req, const SimulationState& state);
};
