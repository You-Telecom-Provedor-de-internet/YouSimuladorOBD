#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <Preferences.h>

#include "config.h"
#include "obd_types.h"
#include "simulation_state.h"
#include "simulation_precedence.h"
#include "vehicle_profiles.h"

void protocol_init(SimulationState* state, SemaphoreHandle_t mutex);
void ui_init(SimulationState* state, SemaphoreHandle_t mutex);
void web_init(SimulationState* state, SemaphoreHandle_t mutex);
void dynamic_init(SimulationState* state, SemaphoreHandle_t mutex);
SimulationState g_sim;
SemaphoreHandle_t g_sim_mutex;

static uint8_t load_saved_protocol(uint8_t fallback_protocol) {
    Preferences prefs;
    prefs.begin("obd", true);
    const uint8_t saved_protocol = prefs.getUChar("proto", 0xFF);
    prefs.end();
    return saved_protocol < PROTO_COUNT ? saved_protocol : fallback_protocol;
}

static bool load_saved_profile_id(char* out, size_t out_size) {
    if (!out || out_size == 0) {
        return false;
    }
    out[0] = '\0';

    Preferences prefs;
    prefs.begin("obd", true);
    String saved_profile = prefs.getString("profile", "");
    prefs.end();

    if (saved_profile.isEmpty()) {
        return false;
    }

    saved_profile.toCharArray(out, out_size);
    return out[0] != '\0';
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[YouSimuladorOBD] Iniciando...");
    simulation_precedence_reset();

    g_sim_mutex = xSemaphoreCreateMutex();
    configASSERT(g_sim_mutex);

    pinMode(PIN_DIP_0, INPUT);
    pinMode(PIN_DIP_1, INPUT);
    pinMode(PIN_DIP_2, INPUT);
    uint8_t dip = (digitalRead(PIN_DIP_2) << 2)
                | (digitalRead(PIN_DIP_1) << 1)
                |  digitalRead(PIN_DIP_0);
    const uint8_t dip_protocol = (dip < PROTO_COUNT) ? dip : PROTO_CAN_11B_500K;
    const uint8_t saved_protocol = load_saved_protocol(dip_protocol);
    if (PROTO_BOOT_OVERRIDE < PROTO_COUNT) {
        g_sim.active_protocol = PROTO_BOOT_OVERRIDE;
        Serial.printf("[OBD] Protocolo inicial travado em: %s (DIP=%s)\n",
                      protoName(g_sim.active_protocol),
                      protoName(dip_protocol));
    } else {
        g_sim.active_protocol = saved_protocol;
        Serial.printf("[OBD] Protocolo inicial: %s (DIP=%s)\n",
                      protoName(g_sim.active_protocol),
                      protoName(dip_protocol));
    }

    char saved_profile_id[sizeof(g_sim.profile_id)] = {};
    if (load_saved_profile_id(saved_profile_id, sizeof(saved_profile_id))) {
        const VehicleProfile* saved_profile = findProfile(saved_profile_id);
        if (saved_profile) {
            applyVehicleProfile(g_sim, *saved_profile);
            simulation_precedence_set_notice(SIM_NOTICE_PROFILE_RESTORED);
            Serial.printf("[VEHICLE] Perfil restaurado: %s %s (%s)\n",
                          saved_profile->brand,
                          saved_profile->model,
                          saved_profile->year);
        } else {
            Serial.printf("[VEHICLE] Perfil salvo invalido ignorado: %s\n", saved_profile_id);
        }
    }

    pinMode(PIN_LED_BUILTIN, OUTPUT);
    pinMode(PIN_LED_CAN, OUTPUT);
    pinMode(PIN_LED_KLINE, OUTPUT);
    pinMode(PIN_LED_TX, OUTPUT);

    protocol_init(&g_sim, g_sim_mutex);
    ui_init(&g_sim, g_sim_mutex);
    web_init(&g_sim, g_sim_mutex);
    dynamic_init(&g_sim, g_sim_mutex);

    Serial.println("[YouSimuladorOBD] Tasks iniciadas.");
}

void loop() {
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(PIN_LED_BUILTIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
}
