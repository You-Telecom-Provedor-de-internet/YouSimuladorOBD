#pragma once
#include <Arduino.h>
#include "simulation_state.h"

void web_init(SimulationState* state, SemaphoreHandle_t mutex);
