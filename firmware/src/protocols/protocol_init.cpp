#include "config.h"
#include "simulation_state.h"
#include "can_protocol.h"
#include "kline_protocol.h"
#include <freertos/semphr.h>
#include <Arduino.h>

// ── Ponto único de entrada para inicialização de protocolo ────

void protocol_init(SimulationState* state, SemaphoreHandle_t mutex) {
    digitalWrite(PIN_LED_CAN, LOW);
    digitalWrite(PIN_LED_KLINE, LOW);

    // Os dois workers sobem juntos e ativam/desativam seu hardware
    // conforme active_protocol muda na UI, web ou perfil.
    can_protocol_init(state, mutex, state->active_protocol);
    kline_protocol_init(state, mutex, state->active_protocol);
}
