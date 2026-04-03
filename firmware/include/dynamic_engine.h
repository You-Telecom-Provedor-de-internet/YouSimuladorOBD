#pragma once
#include "simulation_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Inicia a task FreeRTOS do motor dinâmico.
// Atualiza s_state a cada 100 ms quando sim_mode != SIM_STATIC.
void dynamic_init(SimulationState* state, SemaphoreHandle_t mutex);
void dynamic_persist_odometer_now();
void dynamic_reset_odometer_service_counters(const SimulationState& state);
