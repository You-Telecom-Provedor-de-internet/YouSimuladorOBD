#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "config.h"
#include "simulation_state.h"
#include "obd_types.h"

// ── Módulos (implementados nos demais .cpp) ───────────────────
// Declarações forward — cada módulo expõe uma função init + task
void protocol_init(SimulationState* state, SemaphoreHandle_t mutex);
void ui_init(SimulationState* state, SemaphoreHandle_t mutex);
void web_init(SimulationState* state, SemaphoreHandle_t mutex);

// ── Estado global (acessado por todas as tasks via mutex) ─────
SimulationState  g_sim;
SemaphoreHandle_t g_sim_mutex;

// ════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println("\n[YouSimuladorOBD] Iniciando...");

    // Mutex protege g_sim entre tasks do protocolo e da UI
    g_sim_mutex = xSemaphoreCreateMutex();
    configASSERT(g_sim_mutex);

    // Lê DIP switch para definir protocolo inicial
    pinMode(PIN_DIP_0, INPUT);
    pinMode(PIN_DIP_1, INPUT);
    pinMode(PIN_DIP_2, INPUT);
    uint8_t dip = (digitalRead(PIN_DIP_2) << 2)
                | (digitalRead(PIN_DIP_1) << 1)
                |  digitalRead(PIN_DIP_0);
    g_sim.active_protocol = (dip < PROTO_COUNT) ? dip : 0;
    Serial.printf("[OBD] Protocolo inicial: %s\n", protoName(g_sim.active_protocol));

    // LEDs
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    pinMode(PIN_LED_CAN,     OUTPUT);
    pinMode(PIN_LED_KLINE,   OUTPUT);
    pinMode(PIN_LED_TX,      OUTPUT);

    // Inicializa módulos (cada um cria suas próprias tasks FreeRTOS)
    protocol_init(&g_sim, g_sim_mutex);   // Core 0 — alta prioridade
    ui_init      (&g_sim, g_sim_mutex);   // Core 1 — baixa prioridade
    web_init     (&g_sim, g_sim_mutex);   // Core 1 — Wi-Fi + WebServer

    Serial.println("[YouSimuladorOBD] Tasks iniciadas.");
}

// ── O loop() fica vazio — tudo roda em tasks FreeRTOS ────────
void loop() {
    // Pisca LED builtin como heartbeat (sistema OK)
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(PIN_LED_BUILTIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
}
