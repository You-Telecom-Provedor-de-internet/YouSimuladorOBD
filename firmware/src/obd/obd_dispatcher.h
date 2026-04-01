#pragma once
#include "obd_types.h"
#include "simulation_state.h"

class OBDDispatcher {
public:
    OBDResponse dispatch(const OBDRequest& req, const SimulationState& state);
private:
    void prependModeByte(OBDResponse& resp, uint8_t modeByte);
};
