#pragma once
#include "simulation_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/// Inicia o Bluetooth SPP com emulação ELM327.
/// Roda em paralelo com o protocolo físico (CAN/K-Line).
void elm327_bt_init(SimulationState* state, SemaphoreHandle_t mutex);

/// Retorna true se há um cliente BT conectado.
bool elm327_bt_connected();
