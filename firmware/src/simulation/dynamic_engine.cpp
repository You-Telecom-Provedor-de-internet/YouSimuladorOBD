#include "dynamic_engine.h"
#include "config.h"
#include <Arduino.h>
#include <esp_random.h>
#include <cmath>

// ════════════════════════════════════════════════════════════
//  Motor de simulação dinâmica — atualiza SimulationState
//  a cada 100 ms com física simplificada de motor 1.6L.
//
//  Modos:
//    SIM_IDLE    — marcha lenta estável com microoscilações
//    SIM_URBAN   — stop & go: parado→acelera→cruzeiro→freia
//    SIM_HIGHWAY — cruzeiro 100-120 km/h com ultrapassagens
//    SIM_WARMUP  — cold start: temp sobe de 20°C a 90°C
//    SIM_FAULT   — urbano + falha de ignição periódica (P0300)
// ════════════════════════════════════════════════════════════

static SimulationState*  s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

// ── Helpers matemáticos ───────────────────────────────────────

// Float aleatório em [lo, hi]
static float frand(float lo, float hi) {
    return lo + ((float)(esp_random() & 0x7FFFu) / 32767.0f) * (hi - lo);
}

// Aproximação suave: move current → target no máximo max_step por chamada
static float approach(float cur, float tgt, float max_step) {
    float d = tgt - cur;
    if (fabsf(d) <= max_step) return tgt;
    return cur + (d > 0.0f ? max_step : -max_step);
}

// ── Física do motor ───────────────────────────────────────────

// RPM por km/h em cada marcha (motor 1.6L, relação final ~3.9)
static const float RPM_PER_KMH[6] = { 0.0f, 100.0f, 60.0f, 40.0f, 28.0f, 20.0f };

static uint8_t gear_for_speed(float spd) {
    if (spd < 20.0f) return 1;
    if (spd < 40.0f) return 2;
    if (spd < 65.0f) return 3;
    if (spd < 90.0f) return 4;
    return 5;
}

static float rpm_from_speed(float spd, uint8_t gear) {
    float r = spd * RPM_PER_KMH[gear];
    return r < 750.0f ? 750.0f : r;
}

// MAF g/s: fórmula empírica para 1.6L NA
static float calc_maf(float rpm, float load_pct) {
    return 1.5f + (rpm / 1000.0f) * (load_pct / 100.0f) * 12.0f;
}

// MAP kPa: cresce com TPS
static float calc_map(float tps_pct) {
    return 25.0f + tps_pct * 0.70f;
}

// Avanço de ignição (°)
static float calc_ignition(float rpm) {
    float ign = 8.0f + (rpm - 800.0f) / 300.0f;
    if (ign < 5.0f)  ign = 5.0f;
    if (ign > 25.0f) ign = 25.0f;
    return ign;
}

// Temperatura de admissão: ~30°C em idle, sobe com RPM/carga
static float calc_intake(float coolant, float load_pct) {
    return 25.0f + (coolant - 20.0f) * 0.08f + load_pct * 0.10f;
}

// ── Estado interno do motor ───────────────────────────────────

enum DrivePhase : uint8_t {
    PH_STOPPED = 0,
    PH_ACCEL   = 1,
    PH_CRUISE  = 2,
    PH_BRAKE   = 3,
};

static struct EngState {
    SimMode    prev_mode    = SIM_STATIC;
    DrivePhase phase        = PH_STOPPED;
    uint32_t   phase_end_ms = 0;

    float  target_spd  = 0.0f;
    float  actual_spd  = 0.0f;
    float  actual_rpm  = 800.0f;
    float  actual_tps  = 8.0f;
    float  actual_load = 20.0f;
    float  coolant_f   = 90.0f;
    float  oil_f       = 85.0f;
    float  stft_f      = 0.0f;
    float  ltft_f      = 0.0f;
    float  time_s      = 0.0f;

    // Fault mode
    bool     fault_active  = false;
    uint32_t fault_end_ms  = 0;
    uint32_t next_fault_ms = 0;
} eng;

// ── Transição de modo ─────────────────────────────────────────

static void on_mode_enter(SimMode mode, SimulationState& s) {
    eng.time_s     = 0.0f;
    eng.actual_spd = s.speed_kmh;
    eng.actual_rpm = s.rpm;
    eng.actual_tps = s.throttle_pct;
    eng.actual_load= s.engine_load_pct;
    eng.coolant_f  = (float)s.coolant_temp_c;
    eng.oil_f      = (float)s.oil_temp_c;
    eng.stft_f     = s.stft_pct;
    eng.ltft_f     = s.ltft_pct;

    switch (mode) {
        case SIM_IDLE:
            eng.actual_spd = 0.0f;
            break;

        case SIM_WARMUP:
            eng.coolant_f  = 20.0f;
            eng.oil_f      = 20.0f;
            eng.actual_spd = 0.0f;
            eng.actual_rpm = 950.0f;
            eng.stft_f     = 8.0f;   // mistura rica no cold start
            break;

        case SIM_URBAN:
        case SIM_FAULT:
            eng.actual_spd  = 0.0f;
            eng.phase       = PH_STOPPED;
            eng.phase_end_ms= millis() + (uint32_t)frand(2000.0f, 5000.0f);
            if (mode == SIM_FAULT) {
                eng.fault_active  = false;
                eng.next_fault_ms = millis() + (uint32_t)frand(15000.0f, 35000.0f);
            }
            break;

        case SIM_HIGHWAY:
            eng.actual_spd  = 110.0f;
            eng.actual_rpm  = 2200.0f;
            eng.actual_tps  = 30.0f;
            eng.actual_load = 40.0f;
            eng.phase       = PH_CRUISE;
            eng.target_spd  = 110.0f;
            eng.phase_end_ms= millis() + (uint32_t)frand(60000.0f, 120000.0f);
            break;

        default: break;
    }
}

// ── Update por modo ───────────────────────────────────────────

static void update_idle(SimulationState& s) {
    eng.time_s += 0.1f;

    // RPM oscila levemente (respiração do motor)
    float tgt_rpm = 800.0f + 20.0f * sinf(eng.time_s * 0.5f) + frand(-10.0f, 10.0f);
    eng.actual_rpm = approach(eng.actual_rpm, tgt_rpm, 15.0f);

    eng.actual_tps  = 8.0f  + frand(-2.0f, 2.0f);
    eng.actual_load = 20.0f + frand(-3.0f, 3.0f);
    eng.stft_f     += frand(-0.3f, 0.3f);
    eng.stft_f      = constrain(eng.stft_f, -4.0f, 4.0f);
    eng.ltft_f     += frand(-0.05f, 0.05f);
    eng.ltft_f      = constrain(eng.ltft_f, -3.0f, 3.0f);

    s.rpm             = (uint16_t)eng.actual_rpm;
    s.speed_kmh       = 0;
    s.throttle_pct    = (uint8_t)constrain(eng.actual_tps, 0.0f, 100.0f);
    s.engine_load_pct = (uint8_t)constrain(eng.actual_load, 0.0f, 100.0f);
    s.map_kpa         = (uint8_t)constrain(calc_map(eng.actual_tps) + frand(-2.0f, 2.0f), 20.0f, 105.0f);
    s.maf_gs          = calc_maf(eng.actual_rpm, eng.actual_load) + frand(-0.2f, 0.2f);
    s.ignition_adv    = calc_ignition(eng.actual_rpm) + frand(-0.5f, 0.5f);
    s.intake_temp_c   = (int16_t)calc_intake(eng.coolant_f, eng.actual_load);
    s.coolant_temp_c  = (int16_t)(eng.coolant_f + frand(-0.5f, 0.5f));
    s.oil_temp_c      = (int16_t)(eng.oil_f + frand(-0.3f, 0.3f));
    s.battery_voltage = 14.0f + frand(-0.15f, 0.15f);
    s.stft_pct        = eng.stft_f;
    s.ltft_pct        = eng.ltft_f;
}

static void update_urban_phase(SimulationState& s) {
    uint32_t now = millis();
    uint8_t  gear = gear_for_speed(eng.actual_spd);

    switch (eng.phase) {

        case PH_STOPPED:
            eng.actual_spd  = approach(eng.actual_spd, 0.0f, 1.0f);
            eng.actual_tps  = 5.0f + frand(-1.0f, 1.0f);
            eng.actual_rpm  = approach(eng.actual_rpm, 800.0f, 20.0f);
            eng.actual_load = 18.0f + frand(-2.0f, 2.0f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_ACCEL;
                eng.target_spd  = frand(30.0f, 60.0f);
                eng.actual_tps  = 50.0f;
            }
            break;

        case PH_ACCEL:
            eng.actual_spd  = approach(eng.actual_spd, eng.target_spd, 1.0f);
            eng.actual_tps  = frand(42.0f, 62.0f);
            eng.actual_load = frand(55.0f, 72.0f);
            gear = gear_for_speed(eng.actual_spd);
            eng.actual_rpm  = approach(eng.actual_rpm, rpm_from_speed(eng.actual_spd, gear) + frand(-50.0f, 50.0f), 80.0f);
            if (eng.actual_spd >= eng.target_spd - 1.0f) {
                eng.phase       = PH_CRUISE;
                eng.phase_end_ms= now + (uint32_t)frand(5000.0f, 20000.0f);
            }
            break;

        case PH_CRUISE:
            eng.actual_spd  = approach(eng.actual_spd, eng.target_spd + frand(-2.0f, 2.0f), 0.3f);
            eng.actual_tps  = 28.0f + frand(-4.0f, 4.0f);
            eng.actual_load = 40.0f + frand(-5.0f, 5.0f);
            gear = gear_for_speed(eng.actual_spd);
            eng.actual_rpm  = approach(eng.actual_rpm, rpm_from_speed(eng.actual_spd, gear) + frand(-30.0f, 30.0f), 30.0f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_BRAKE;
                eng.phase_end_ms= now + (uint32_t)frand(4000.0f, 8000.0f);
            }
            break;

        case PH_BRAKE:
            eng.actual_spd  = approach(eng.actual_spd, 0.0f, 1.5f);
            eng.actual_tps  = frand(0.0f, 3.0f);
            eng.actual_load = 10.0f + frand(-2.0f, 2.0f);
            gear = gear_for_speed(eng.actual_spd);
            eng.actual_rpm  = approach(eng.actual_rpm, rpm_from_speed(eng.actual_spd, gear), 60.0f);
            if (eng.actual_spd <= 1.0f) {
                eng.actual_spd  = 0.0f;
                eng.phase       = PH_STOPPED;
                eng.phase_end_ms= now + (uint32_t)frand(3000.0f, 8000.0f);
            }
            break;
    }

    eng.stft_f += frand(-0.25f, 0.25f);
    eng.stft_f  = constrain(eng.stft_f, -5.0f, 5.0f);
    eng.ltft_f += frand(-0.04f, 0.04f);
    eng.ltft_f  = constrain(eng.ltft_f, -3.0f, 3.0f);

    s.rpm             = (uint16_t)constrain(eng.actual_rpm, 0.0f, 16000.0f);
    s.speed_kmh       = (uint8_t) constrain(eng.actual_spd, 0.0f, 255.0f);
    s.throttle_pct    = (uint8_t) constrain(eng.actual_tps, 0.0f, 100.0f);
    s.engine_load_pct = (uint8_t) constrain(eng.actual_load, 0.0f, 100.0f);
    s.map_kpa         = (uint8_t) constrain(calc_map(eng.actual_tps) + frand(-2.0f, 2.0f), 20.0f, 105.0f);
    s.maf_gs          = constrain(calc_maf(eng.actual_rpm, eng.actual_load) + frand(-0.3f, 0.3f), 0.0f, 655.0f);
    s.ignition_adv    = calc_ignition(eng.actual_rpm) + frand(-0.5f, 0.5f);
    s.intake_temp_c   = (int16_t)calc_intake(eng.coolant_f, eng.actual_load);
    s.coolant_temp_c  = (int16_t)(eng.coolant_f + frand(-0.5f, 0.5f));
    s.oil_temp_c      = (int16_t)(eng.oil_f + frand(-0.3f, 0.3f));
    s.battery_voltage = 13.8f + frand(-0.2f, 0.3f);
    s.stft_pct        = eng.stft_f;
    s.ltft_pct        = eng.ltft_f;
}

static void update_highway(SimulationState& s) {
    uint32_t now = millis();

    switch (eng.phase) {
        case PH_CRUISE:
            eng.target_spd = 110.0f + frand(-5.0f, 8.0f);
            eng.actual_spd = approach(eng.actual_spd, eng.target_spd, 0.3f);
            eng.actual_tps = 30.0f + frand(-3.0f, 3.0f);
            eng.actual_load= 40.0f + frand(-4.0f, 4.0f);
            if (now >= eng.phase_end_ms) {          // início ultrapassagem
                eng.phase       = PH_ACCEL;
                eng.target_spd  = 130.0f;
                eng.phase_end_ms= now + (uint32_t)frand(8000.0f, 15000.0f);
            }
            break;

        case PH_ACCEL:                              // ultrapassagem
            eng.actual_spd = approach(eng.actual_spd, eng.target_spd, 0.8f);
            eng.actual_tps = 60.0f + frand(-5.0f, 5.0f);
            eng.actual_load= 65.0f + frand(-5.0f, 5.0f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_CRUISE;
                eng.target_spd  = 110.0f;
                eng.phase_end_ms= now + (uint32_t)frand(60000.0f, 120000.0f);
            }
            break;

        default:
            eng.phase       = PH_CRUISE;
            eng.phase_end_ms= now + (uint32_t)frand(60000.0f, 120000.0f);
            break;
    }

    uint8_t gear = gear_for_speed(eng.actual_spd);
    eng.actual_rpm = approach(eng.actual_rpm, rpm_from_speed(eng.actual_spd, gear) + frand(-40.0f, 40.0f), 40.0f);
    eng.stft_f    += frand(-0.2f, 0.2f);
    eng.stft_f     = constrain(eng.stft_f, -4.0f, 4.0f);
    eng.ltft_f    += frand(-0.03f, 0.03f);
    eng.ltft_f     = constrain(eng.ltft_f, -3.0f, 3.0f);

    s.rpm             = (uint16_t)constrain(eng.actual_rpm, 0.0f, 16000.0f);
    s.speed_kmh       = (uint8_t) constrain(eng.actual_spd, 0.0f, 255.0f);
    s.throttle_pct    = (uint8_t) constrain(eng.actual_tps, 0.0f, 100.0f);
    s.engine_load_pct = (uint8_t) constrain(eng.actual_load, 0.0f, 100.0f);
    s.map_kpa         = (uint8_t) constrain(calc_map(eng.actual_tps) + frand(-2.0f, 2.0f), 20.0f, 105.0f);
    s.maf_gs          = constrain(calc_maf(eng.actual_rpm, eng.actual_load) + frand(-0.3f, 0.3f), 0.0f, 655.0f);
    s.ignition_adv    = calc_ignition(eng.actual_rpm) + frand(-0.5f, 0.5f);
    s.intake_temp_c   = (int16_t)calc_intake(eng.coolant_f, eng.actual_load);
    s.coolant_temp_c  = (int16_t)(eng.coolant_f + frand(-0.5f, 0.5f));
    s.oil_temp_c      = (int16_t)(eng.oil_f + frand(-0.3f, 0.3f));
    s.battery_voltage = 13.8f + frand(-0.15f, 0.15f);
    s.stft_pct        = eng.stft_f;
    s.ltft_pct        = eng.ltft_f;
}

static void update_warmup(SimulationState& s) {
    // Coolant sobe ~0.12°C/s (90°C em ~583s ≈ 10 min)
    if (eng.coolant_f < 90.0f) {
        eng.coolant_f += 0.012f;   // 0.012°C por 100ms = 0.12°C/s
        if (eng.coolant_f > 90.0f) eng.coolant_f = 90.0f;
    }
    // Óleo aquece mais devagar (~0.06°C/s)
    if (eng.oil_f < 85.0f) {
        eng.oil_f += 0.006f;
        if (eng.oil_f > 85.0f) eng.oil_f = 85.0f;
    }

    // RPM diminui de 950→800 conforme motor aquece (0°C→70°C)
    float rpm_target = 800.0f;
    if (eng.coolant_f < 70.0f) {
        float t = eng.coolant_f / 70.0f;          // 0.0→1.0 conforme esquenta
        rpm_target = 950.0f - t * 150.0f;         // 950→800
    }
    eng.actual_rpm = approach(eng.actual_rpm, rpm_target + frand(-15.0f, 15.0f), 10.0f);

    // STFT: +8% no frio, normaliza ao chegar em 70°C
    float stft_target = 0.0f;
    if (eng.coolant_f < 70.0f) {
        stft_target = 8.0f * (1.0f - eng.coolant_f / 70.0f);
    }
    eng.stft_f = approach(eng.stft_f, stft_target + frand(-0.5f, 0.5f), 0.1f);

    eng.actual_tps  = 10.0f + frand(-2.0f, 2.0f);
    eng.actual_load = 22.0f + frand(-3.0f, 3.0f);

    s.rpm             = (uint16_t)eng.actual_rpm;
    s.speed_kmh       = 0;
    s.throttle_pct    = (uint8_t)constrain(eng.actual_tps, 0.0f, 100.0f);
    s.engine_load_pct = (uint8_t)constrain(eng.actual_load, 0.0f, 100.0f);
    s.map_kpa         = (uint8_t)constrain(calc_map(eng.actual_tps) + frand(-2.0f, 2.0f), 20.0f, 105.0f);
    s.maf_gs          = calc_maf(eng.actual_rpm, eng.actual_load) + frand(-0.2f, 0.2f);
    s.ignition_adv    = calc_ignition(eng.actual_rpm) + frand(-0.5f, 0.5f);
    s.intake_temp_c   = (int16_t)(20.0f + (eng.coolant_f - 20.0f) * 0.15f);
    s.coolant_temp_c  = (int16_t)eng.coolant_f;
    s.oil_temp_c      = (int16_t)eng.oil_f;
    s.battery_voltage = 14.0f + frand(-0.15f, 0.15f);
    s.stft_pct        = eng.stft_f;
    s.ltft_pct        = 0.0f;

    // Ao atingir temperatura, não desativa o modo — deixa estabilizado em idle
}

static void update_fault(SimulationState& s) {
    uint32_t now = millis();

    // Verificar transição de falha
    if (!eng.fault_active && now >= eng.next_fault_ms) {
        eng.fault_active = true;
        eng.fault_end_ms = now + (uint32_t)frand(10000.0f, 20000.0f);
        // Adiciona DTC P0300
        s.dtcs[0]  = 0x0300;
        s.dtc_count = 1;
    }
    if (eng.fault_active && now >= eng.fault_end_ms) {
        eng.fault_active  = false;
        eng.next_fault_ms = now + (uint32_t)frand(25000.0f, 50000.0f);
        // Remove DTC P0300
        s.dtc_count = 0;
    }

    // Roda a máquina urbana como base
    update_urban_phase(s);

    // Sobrepõe comportamento de falha
    if (eng.fault_active) {
        // RPM errático (misfire)
        s.rpm = (uint16_t)constrain((int)s.rpm + (int)frand(-180.0f, 180.0f), 500, 16000);
        // STFT sobe (ECU tenta compensar)
        eng.stft_f = approach(eng.stft_f, 16.0f + frand(-2.0f, 2.0f), 0.5f);
        s.stft_pct = eng.stft_f;
        // Carga sobe (combustão ineficiente)
        s.engine_load_pct = (uint8_t)constrain(s.engine_load_pct + 10, 0, 100);
    }
}

// ── Task FreeRTOS ─────────────────────────────────────────────

static void task_dynamic_engine(void*) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 Hz

        xSemaphoreTake(s_mutex, portMAX_DELAY);

        SimMode mode = s_state->sim_mode;

        if (mode != SIM_STATIC) {
            // Detecta troca de modo e inicializa estado interno
            if (mode != eng.prev_mode) {
                on_mode_enter(mode, *s_state);
                eng.prev_mode = mode;
            }

            switch (mode) {
                case SIM_IDLE:    update_idle(*s_state);           break;
                case SIM_URBAN:   update_urban_phase(*s_state);    break;
                case SIM_HIGHWAY: update_highway(*s_state);        break;
                case SIM_WARMUP:  update_warmup(*s_state);         break;
                case SIM_FAULT:   update_fault(*s_state);          break;
                default: break;
            }
        } else {
            // Ao voltar para STATIC, registra o modo para detectar
            // a próxima ativação corretamente
            eng.prev_mode = SIM_STATIC;
        }

        xSemaphoreGive(s_mutex);
    }
}

// ── Init público ──────────────────────────────────────────────

void dynamic_init(SimulationState* state, SemaphoreHandle_t mutex) {
    s_state = state;
    s_mutex = mutex;
    xTaskCreatePinnedToCore(
        task_dynamic_engine, "dyn_engine",
        4096, nullptr, 1, nullptr, 1);
}
