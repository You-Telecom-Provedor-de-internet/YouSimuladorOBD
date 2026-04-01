#pragma once
#include "obd_types.h"
#include "simulation_state.h"

class Mode03Handler {
public:
    OBDResponse handle(const SimulationState& state);
};
