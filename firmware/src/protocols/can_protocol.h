#pragma once
#include <Arduino.h>
#include "simulation_state.h"
#include "obd_dispatcher.h"

void can_protocol_init(SimulationState* state, SemaphoreHandle_t mutex, uint8_t protocol);
