#include "config.h"
#include "simulation_state.h"
#include "can_protocol.h"
#include "kline_protocol.h"
#include <freertos/semphr.h>
#include <Arduino.h>

// ── Ponto único de entrada para inicialização de protocolo ────

void protocol_init(SimulationState* state, SemaphoreHandle_t mutex) {
    uint8_t proto = state->active_protocol;

    if (proto <= PROTO_CAN_29B_250K) {
        digitalWrite(PIN_LED_CAN,   HIGH);
        digitalWrite(PIN_LED_KLINE, LOW);
        can_protocol_init(state, mutex, proto);
    } else {
        digitalWrite(PIN_LED_CAN,   LOW);
        digitalWrite(PIN_LED_KLINE, HIGH);
        kline_protocol_init(state, mutex, proto);
    }
}
