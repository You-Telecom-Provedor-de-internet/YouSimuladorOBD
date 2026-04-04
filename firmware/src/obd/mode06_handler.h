#pragma once
#include "obd_types.h"
#include "simulation_state.h"

class Mode06Handler {
public:
    OBDResponse handle(const OBDRequest& req, const SimulationState& state);
};
