#include "dynamic_engine.h"
#include "config.h"
#include <Arduino.h>
#include <esp_random.h>
#include <cmath>

// ════════════════════════════════════════════════════════════
//  Motor de simulação dinâmica v2 — calibrado com dados reais
//  Motor referência: 1.6L 4 cilindros NA, câmbio 5 marchas
//
//  Fontes de calibração (valores reais):
//    Idle:  RPM 780-820, MAF 3-5 g/s, MAP 25-35 kPa, Load 15-20%
//    Urban: RPM 800-3500, MAF 3-25 g/s, TPS 0-50%, Load 15-70%
//    Hwy:   RPM 2000-2500, MAF 15-22 g/s, TPS 15-25%, Load 30-50%
//    Warmup: coolant 20→90°C em ~5-10 min (comprimido p/ ~3 min)
//    Oil lags coolant ~10-15°C, atinge temp ~10 min após coolant
//    STFT normal: ±5%, cold start: +8% a +15% (open loop)
//    LTFT normal: ±5%, adapta lentamente após closed loop
//    Battery: alternador 13.8-14.4V
// ════════════════════════════════════════════════════════════

static SimulationState*  s_state = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;

// ── Helpers matemáticos ───────────────────────────────────────

static float frand(float lo, float hi) {
    return lo + ((float)(esp_random() & 0x7FFFu) / 32767.0f) * (hi - lo);
}

// Interpolação suave: move current → target limitado a max_step/tick
static float approach(float cur, float tgt, float max_step) {
    float d = tgt - cur;
    if (fabsf(d) <= max_step) return tgt;
    return cur + (d > 0.0f ? max_step : -max_step);
}

// Exponential moving average (suaviza transições)
static float ema(float cur, float tgt, float alpha) {
    return cur + alpha * (tgt - cur);
}

// Noise suave: random walk com reversão à média (evita flickering)
struct Wander {
    float v = 0.0f;
    // pull = força de retorno a zero (0.05=lento, 0.3=rápido)
    // amp  = amplitude do ruído por tick
    void step(float pull, float amp) {
        v = v * (1.0f - pull) + frand(-amp, amp);
    }
};

// ── Física do motor 1.6L ─────────────────────────────────────

// RPM por km/h em cada marcha (relação final ~3.94)
//   1ª: 0-20 km/h, 2ª: 20-40, 3ª: 40-65, 4ª: 65-90, 5ª: 90+
static const float RPM_PER_KMH[6] = { 0, 105, 62, 42, 30, 22 };

static uint8_t gear_for_speed(float spd) {
    if (spd < 15) return 1;
    if (spd < 35) return 2;
    if (spd < 60) return 3;
    if (spd < 85) return 4;
    return 5;
}

static float rpm_from_speed(float spd) {
    if (spd < 1.0f) return 780.0f;  // idle quando parado
    uint8_t g = gear_for_speed(spd);
    float r = spd * RPM_PER_KMH[g];
    return r < 780.0f ? 780.0f : r;
}

// MAF g/s: calibrado — idle ~4 g/s, cruise 60 km/h ~12 g/s, WOT ~55 g/s
static float calc_maf(float rpm, float load_pct) {
    return 1.8f + (rpm / 1000.0f) * (load_pct / 100.0f) * 13.0f;
}

// MAP kPa: vácuo no idle (~30 kPa), sobe com TPS até ~98 kPa WOT
static float calc_map(float tps_pct) {
    return 27.0f + tps_pct * 0.71f;
}

// Avanço de ignição: depende de RPM e carga
// Mais avanço em baixa carga e RPM médio, retarda sob carga alta
static float calc_ignition(float rpm, float load_pct) {
    float base = 10.0f + (rpm - 800.0f) / 400.0f;
    float load_retard = load_pct * 0.06f;
    return constrain(base - load_retard, 5.0f, 32.0f);
}

// Temperatura de admissão: ambiente + calor do motor
static float calc_intake(float coolant, float load_pct) {
    float heat_soak = (coolant - 20.0f) * 0.07f;
    float load_heat = load_pct * 0.06f;
    return constrain(25.0f + heat_soak + load_heat, -40.0f, 80.0f);
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

    float target_spd  = 0;
    float actual_spd  = 0;
    float actual_rpm  = 800;
    float actual_tps  = 3;
    float actual_load = 18;
    float coolant_f   = 90;
    float oil_f       = 85;
    float stft_f      = 0;
    float ltft_f      = 0;
    float fuel_f      = 75;
    float bat_f       = 14.1f;
    float time_s      = 0;

    // Canais de ruído suave (sem flickering)
    Wander w_rpm, w_tps, w_load, w_stft, w_cool, w_oil, w_bat;

    // Estado de falha (modo FAULT)
    bool     fault_active   = false;
    uint32_t fault_toggle_ms= 0;
} eng;

// ── Noise suave (chamado todo tick) ──────────────────────────

static void update_noise() {
    eng.w_rpm.step(0.08f,  6.0f);    // RPM: wander ±~15 RPM
    eng.w_tps.step(0.15f,  0.4f);    // TPS: ±~0.7%
    eng.w_load.step(0.10f, 0.8f);    // Load: ±~1.8%
    eng.w_stft.step(0.06f, 0.2f);    // STFT: ±~0.6% (oscilação lenta do O2)
    eng.w_cool.step(0.25f, 0.15f);   // Coolant: ±~0.2°C
    eng.w_oil.step(0.25f, 0.10f);    // Oil: ±~0.15°C
    eng.w_bat.step(0.12f, 0.03f);    // Battery: ±~0.06V
}

// ── Aplica estado do motor → SimulationState (output comum) ──

static void write_state(SimulationState& s) {
    s.rpm             = (uint16_t)constrain(eng.actual_rpm + eng.w_rpm.v, 0.0f, 16000.0f);
    s.speed_kmh       = (uint8_t) constrain(eng.actual_spd, 0.0f, 255.0f);
    s.throttle_pct    = (uint8_t) constrain(eng.actual_tps + eng.w_tps.v, 0.0f, 100.0f);
    s.engine_load_pct = (uint8_t) constrain(eng.actual_load + eng.w_load.v, 0.0f, 100.0f);
    s.map_kpa         = (uint8_t) constrain(calc_map(eng.actual_tps), 20.0f, 105.0f);
    s.maf_gs          = constrain(calc_maf(eng.actual_rpm, eng.actual_load), 0.0f, 655.0f);
    s.ignition_adv    = calc_ignition(eng.actual_rpm, eng.actual_load);
    s.intake_temp_c   = (int16_t) calc_intake(eng.coolant_f, eng.actual_load);
    s.coolant_temp_c  = (int16_t)(eng.coolant_f + eng.w_cool.v);
    s.oil_temp_c      = (int16_t)(eng.oil_f + eng.w_oil.v);
    s.battery_voltage = eng.bat_f + eng.w_bat.v;
    s.stft_pct        = eng.stft_f + eng.w_stft.v;
    s.ltft_pct        = eng.ltft_f;
    s.fuel_level_pct  = (uint8_t) constrain(eng.fuel_f, 0.0f, 100.0f);
}

// ── Temperatura do motor aquecido (estabilização termostatada) ─

static void update_warm_engine_temps() {
    // Motor aquecido: coolant regulado pelo termostato entre 88-95°C
    eng.coolant_f = ema(eng.coolant_f, 92.0f, 0.005f);
    // Oil segue coolant com atraso, estabiliza ~87°C
    eng.oil_f = ema(eng.oil_f, 87.0f, 0.003f);
}

// ── Transição de modo ─────────────────────────────────────────

static void on_mode_enter(SimMode mode, SimulationState& s) {
    eng.time_s = 0;

    // Reset noise channels
    eng.w_rpm.v = eng.w_tps.v = eng.w_load.v = 0;
    eng.w_stft.v = eng.w_cool.v = eng.w_oil.v = eng.w_bat.v = 0;

    switch (mode) {
        case SIM_IDLE:
            eng.actual_spd  = 0;
            eng.actual_rpm  = 800;
            eng.actual_tps  = 3;
            eng.actual_load = 18;
            eng.coolant_f   = 92;
            eng.oil_f       = 87;
            eng.stft_f      = 0;
            eng.ltft_f      = 0;
            eng.bat_f       = 14.1f;
            eng.fuel_f      = s.fuel_level_pct;
            break;

        case SIM_WARMUP:
            eng.actual_spd  = 0;
            eng.actual_rpm  = 1100;   // fast idle (choke)
            eng.actual_tps  = 8;
            eng.actual_load = 25;
            eng.coolant_f   = 20;     // ambiente
            eng.oil_f       = 20;     // ambiente
            eng.stft_f      = 12;     // open loop: mistura rica
            eng.ltft_f      = 3;      // LTFT ligeiramente positivo
            eng.bat_f       = 14.2f;
            eng.fuel_f      = s.fuel_level_pct;
            // Limpa DTCs (cold start é normal)
            s.dtc_count = 0;
            break;

        case SIM_URBAN:
            eng.actual_spd  = 0;
            eng.actual_rpm  = 800;
            eng.actual_tps  = 3;
            eng.actual_load = 18;
            eng.coolant_f   = 92;
            eng.oil_f       = 87;
            eng.stft_f      = 0;
            eng.ltft_f      = 0;
            eng.bat_f       = 14.0f;
            eng.fuel_f      = s.fuel_level_pct;
            eng.phase        = PH_STOPPED;
            eng.phase_end_ms = millis() + (uint32_t)frand(2000, 6000);
            break;

        case SIM_HIGHWAY:
            eng.actual_spd  = 110;
            eng.actual_rpm  = 110 * RPM_PER_KMH[5];  // 5ª marcha
            eng.actual_tps  = 22;
            eng.actual_load = 38;
            eng.coolant_f   = 93;
            eng.oil_f       = 90;
            eng.stft_f      = -1.5f;  // ligeiramente lean em cruise
            eng.ltft_f      = 0;
            eng.bat_f       = 14.0f;
            eng.fuel_f      = s.fuel_level_pct;
            eng.phase        = PH_CRUISE;
            eng.target_spd   = 110;
            eng.phase_end_ms = millis() + (uint32_t)frand(60000, 120000);
            break;

        case SIM_FAULT:
            // Motor aquecido com falha IMEDIATA
            eng.actual_spd  = 0;
            eng.actual_rpm  = 780;
            eng.actual_tps  = 4;
            eng.actual_load = 22;
            eng.coolant_f   = 92;
            eng.oil_f       = 87;
            eng.stft_f      = 15;     // ECU compensando misfire
            eng.ltft_f      = 8;      // LTFT alto (falha persistente)
            eng.bat_f       = 13.9f;
            eng.fuel_f      = s.fuel_level_pct;
            eng.phase        = PH_STOPPED;
            eng.phase_end_ms = millis() + (uint32_t)frand(3000, 7000);
            // DTCs imediatos: P0300 (random misfire) + P0301 (cyl 1) + P0304 (cyl 4)
            eng.fault_active   = true;
            eng.fault_toggle_ms= millis() + (uint32_t)frand(30000, 60000);
            s.dtc_count = 3;
            s.dtcs[0] = 0x0300;  // P0300 — Random/Multiple cylinder misfire
            s.dtcs[1] = 0x0301;  // P0301 — Cylinder 1 misfire
            s.dtcs[2] = 0x0304;  // P0304 — Cylinder 4 misfire
            break;

        default: break;
    }
}

// ════════════════════════════════════════════════════════════
//  IDLE — motor aquecido, parado, microoscilações suaves
// ════════════════════════════════════════════════════════════

static void update_idle(SimulationState& s) {
    eng.time_s += 0.1f;

    // RPM: respiração suave do motor (senoide lenta + wander)
    float breath = 10.0f * sinf(eng.time_s * 0.4f);  // ±10 RPM, período ~16s
    eng.actual_rpm = ema(eng.actual_rpm, 800.0f + breath, 0.08f);

    eng.actual_tps  = ema(eng.actual_tps, 3.0f, 0.05f);
    eng.actual_load = ema(eng.actual_load, 18.0f, 0.05f);

    // Fuel trim: oscilação lenta do sensor O2 (normal em closed loop)
    eng.stft_f = ema(eng.stft_f, 0.5f * sinf(eng.time_s * 0.3f), 0.03f);
    eng.ltft_f = ema(eng.ltft_f, 0.0f, 0.002f);

    eng.bat_f = ema(eng.bat_f, 14.1f, 0.02f);
    eng.fuel_f -= 0.0003f;  // consumo mínimo em idle

    update_warm_engine_temps();
    write_state(s);
}

// ════════════════════════════════════════════════════════════
//  URBAN — stop & go: parado→acelera→cruzeiro→freia→parado
//  Vel.: 0-60 km/h, RPM 800-3500
// ════════════════════════════════════════════════════════════

static void update_urban(SimulationState& s) {
    uint32_t now = millis();

    switch (eng.phase) {

        case PH_STOPPED:
            // Parado no sinal — idle
            eng.actual_spd  = approach(eng.actual_spd, 0, 0.5f);
            eng.actual_rpm  = ema(eng.actual_rpm, 800, 0.06f);
            eng.actual_tps  = ema(eng.actual_tps, 3, 0.08f);
            eng.actual_load = ema(eng.actual_load, 18, 0.06f);
            if (now >= eng.phase_end_ms) {
                eng.phase      = PH_ACCEL;
                eng.target_spd = frand(30, 55);  // 30-55 km/h urbano
            }
            break;

        case PH_ACCEL: {
            // Aceleração: ~0.8 km/h por 100ms = 0-50 em ~6s (realista)
            float accel_rate = frand(0.6f, 1.0f);
            eng.actual_spd = approach(eng.actual_spd, eng.target_spd, accel_rate);
            // TPS sobe durante aceleração (15-45% dependendo da intensidade)
            float tps_target = 18.0f + (eng.target_spd / 60.0f) * 30.0f;
            eng.actual_tps  = ema(eng.actual_tps, tps_target, 0.12f);
            // Carga alta durante aceleração
            eng.actual_load = ema(eng.actual_load, 45.0f + eng.actual_tps * 0.5f, 0.08f);
            // RPM segue marcha
            eng.actual_rpm  = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.10f);
            if (eng.actual_spd >= eng.target_spd - 1.0f) {
                eng.phase       = PH_CRUISE;
                eng.phase_end_ms= now + (uint32_t)frand(8000, 25000);
            }
            break;
        }

        case PH_CRUISE:
            // Cruzeiro urbano: velocidade estável, baixo TPS
            eng.actual_spd  = ema(eng.actual_spd, eng.target_spd, 0.03f);
            eng.actual_tps  = ema(eng.actual_tps, 12.0f + eng.target_spd * 0.15f, 0.06f);
            eng.actual_load = ema(eng.actual_load, 28.0f + eng.target_spd * 0.3f, 0.04f);
            eng.actual_rpm  = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.06f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_BRAKE;
                eng.phase_end_ms= now + (uint32_t)frand(4000, 8000);
            }
            break;

        case PH_BRAKE:
            // Frenagem: ~1.2 km/h por 100ms = 50-0 em ~4s
            eng.actual_spd = approach(eng.actual_spd, 0, 1.2f);
            // Engine braking: TPS fecha, RPM cai com velocidade
            eng.actual_tps  = ema(eng.actual_tps, 0, 0.15f);
            eng.actual_load = ema(eng.actual_load, 8, 0.10f);
            eng.actual_rpm  = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.08f);
            if (eng.actual_spd < 2.0f) {
                eng.actual_spd   = 0;
                eng.phase        = PH_STOPPED;
                eng.phase_end_ms = now + (uint32_t)frand(3000, 8000);
            }
            break;
    }

    // Fuel trim: oscilação suave em closed loop
    eng.stft_f = ema(eng.stft_f, 1.5f * sinf(eng.time_s * 0.25f), 0.02f);
    eng.ltft_f = ema(eng.ltft_f, 0, 0.001f);

    eng.bat_f = ema(eng.bat_f, 14.0f, 0.02f);
    eng.fuel_f -= 0.0008f;  // consumo urbano ~8 L/100km
    eng.time_s += 0.1f;

    update_warm_engine_temps();
    write_state(s);
}

// ════════════════════════════════════════════════════════════
//  HIGHWAY — cruzeiro 110 km/h com ultrapassagens ocasionais
// ════════════════════════════════════════════════════════════

static void update_highway(SimulationState& s) {
    uint32_t now = millis();

    switch (eng.phase) {
        case PH_CRUISE: {
            // Cruzeiro 5ª marcha ~110 km/h
            float target = 110.0f + 3.0f * sinf(eng.time_s * 0.08f); // micro-variação
            eng.actual_spd  = ema(eng.actual_spd, target, 0.04f);
            eng.actual_tps  = ema(eng.actual_tps, 22, 0.05f);
            eng.actual_load = ema(eng.actual_load, 38, 0.04f);
            if (now >= eng.phase_end_ms) {
                // Inicia ultrapassagem
                eng.phase       = PH_ACCEL;
                eng.target_spd  = frand(125, 135);
                eng.phase_end_ms= now + (uint32_t)frand(8000, 15000);
            }
            break;
        }

        case PH_ACCEL:
            // Ultrapassagem: acelera para 130 km/h
            eng.actual_spd  = approach(eng.actual_spd, eng.target_spd, 0.6f);
            eng.actual_tps  = ema(eng.actual_tps, 55, 0.10f);
            eng.actual_load = ema(eng.actual_load, 65, 0.08f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_BRAKE;
                eng.phase_end_ms= now + (uint32_t)frand(5000, 10000);
            }
            break;

        case PH_BRAKE:
            // Retorna ao cruzeiro suavemente
            eng.actual_spd  = approach(eng.actual_spd, 110, 0.4f);
            eng.actual_tps  = ema(eng.actual_tps, 22, 0.06f);
            eng.actual_load = ema(eng.actual_load, 38, 0.05f);
            if (eng.actual_spd <= 112.0f) {
                eng.phase       = PH_CRUISE;
                eng.phase_end_ms= now + (uint32_t)frand(60000, 120000);
            }
            break;

        default:
            eng.phase       = PH_CRUISE;
            eng.phase_end_ms= now + 60000;
            break;
    }

    eng.actual_rpm = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.08f);

    // Fuel trim: ligeiramente lean em cruise (eficiência)
    eng.stft_f = ema(eng.stft_f, -1.2f + sinf(eng.time_s * 0.2f), 0.02f);
    eng.ltft_f = ema(eng.ltft_f, 0, 0.001f);

    eng.bat_f = ema(eng.bat_f, 14.0f, 0.02f);
    eng.fuel_f -= 0.0006f;  // consumo estrada ~6 L/100km
    eng.time_s += 0.1f;

    update_warm_engine_temps();
    write_state(s);
}

// ════════════════════════════════════════════════════════════
//  WARMUP — arranque a frio: 20°C → 90°C em ~3 minutos
//  Curva realista em 4 fases:
//    Fase 1 (0-60s):   20→55°C  — termostato fechado, aquece rápido
//    Fase 2 (60-120s): 55→80°C  — termostato abrindo
//    Fase 3 (120-180s):80→90°C  — radiador ativo, estabiliza
//    Fase 4 (180s+):   90-93°C  — regulado pelo termostato
//  Oil: lags coolant ~12°C, sobe a ~65% da taxa do coolant
// ════════════════════════════════════════════════════════════

static void update_warmup(SimulationState& s) {
    eng.time_s += 0.1f;

    // ── Coolant: curva de 4 fases ─────────────────────────────
    float coolant_rate;
    if (eng.coolant_f < 55.0f) {
        // Fase 1: termostato fechado → aquece rápido (~0.058°C/tick = 35°C em 60s)
        coolant_rate = 0.058f;
    } else if (eng.coolant_f < 80.0f) {
        // Fase 2: termostato abrindo → desacelera (~0.042°C/tick = 25°C em 60s)
        coolant_rate = 0.042f;
    } else if (eng.coolant_f < 90.0f) {
        // Fase 3: radiador ativo → bem mais lento (~0.017°C/tick = 10°C em 60s)
        coolant_rate = 0.017f;
    } else {
        // Fase 4: termostato regulando — oscila 90-93°C
        coolant_rate = 0.0f;
        eng.coolant_f = ema(eng.coolant_f, 92.0f + sinf(eng.time_s * 0.15f) * 1.5f, 0.01f);
    }
    if (coolant_rate > 0.0f) {
        eng.coolant_f += coolant_rate;
    }

    // ── Oil: lags coolant, sobe mais devagar ──────────────────
    float oil_target = eng.coolant_f - 12.0f;  // sempre ~12°C atrás
    if (oil_target < 20.0f) oil_target = 20.0f;
    if (oil_target > 87.0f) oil_target = 87.0f;
    eng.oil_f = ema(eng.oil_f, oil_target, 0.008f);  // resposta lenta

    // ── RPM: fast idle → idle normal ──────────────────────────
    // 1100 RPM a frio → 950 a 50°C → 850 a 70°C → 800 a 90°C
    float rpm_target;
    if (eng.coolant_f < 50.0f) {
        float t = (eng.coolant_f - 20.0f) / 30.0f;  // 0→1 de 20°C a 50°C
        rpm_target = 1100.0f - t * 150.0f;           // 1100→950
    } else if (eng.coolant_f < 70.0f) {
        float t = (eng.coolant_f - 50.0f) / 20.0f;  // 0→1 de 50°C a 70°C
        rpm_target = 950.0f - t * 100.0f;            // 950→850
    } else {
        float t = constrain((eng.coolant_f - 70.0f) / 20.0f, 0.0f, 1.0f);
        rpm_target = 850.0f - t * 50.0f;             // 850→800
    }
    eng.actual_rpm = ema(eng.actual_rpm, rpm_target, 0.04f);

    // ── TPS e carga: diminuem conforme motor aquece ───────────
    float temp_factor = constrain((eng.coolant_f - 20.0f) / 70.0f, 0.0f, 1.0f);
    eng.actual_tps  = ema(eng.actual_tps, 8.0f - temp_factor * 5.0f, 0.03f);  // 8%→3%
    eng.actual_load = ema(eng.actual_load, 25.0f - temp_factor * 7.0f, 0.03f); // 25%→18%

    // ── Fuel trim: open loop rico → closed loop normal ────────
    // Open loop (~20s): STFT fixo +12%, depois diminui gradualmente
    if (eng.time_s < 2.0f) {
        // Primeiros 20s: open loop, sem sensor O2
        eng.stft_f = 12.0f;
    } else if (eng.coolant_f < 60.0f) {
        // Closed loop mas motor frio: STFT gradualmente diminui
        float warm_progress = (eng.coolant_f - 20.0f) / 40.0f;  // 0→1 de 20→60°C
        eng.stft_f = ema(eng.stft_f, 12.0f * (1.0f - warm_progress * 0.7f), 0.02f);
    } else {
        // Motor quente: STFT normaliza para ±2%
        eng.stft_f = ema(eng.stft_f, 0.5f * sinf(eng.time_s * 0.3f), 0.02f);
    }
    // LTFT adapta lentamente depois de 30s
    if (eng.time_s > 3.0f) {
        eng.ltft_f = ema(eng.ltft_f, 0, 0.003f);
    }

    eng.bat_f = ema(eng.bat_f, 14.2f, 0.02f);
    eng.fuel_f -= 0.0004f;  // consumo cold idle (mais que warm idle)

    write_state(s);
}

// ════════════════════════════════════════════════════════════
//  FAULT — falha de ignição intermitente (P0300/P0301/P0304)
//  DTCs ativos imediatamente. RPM irregular, STFT alto.
//  Alterna entre períodos de falha intensa e falha leve.
// ════════════════════════════════════════════════════════════

static void update_fault(SimulationState& s) {
    uint32_t now = millis();
    eng.time_s += 0.1f;

    // ── Alterna entre falha intensa e falha leve ──────────────
    if (now >= eng.fault_toggle_ms) {
        eng.fault_active = !eng.fault_active;
        if (eng.fault_active) {
            // Período de falha intensa: 15-30s
            eng.fault_toggle_ms = now + (uint32_t)frand(15000, 30000);
            // DTCs aparecem
            s.dtc_count = 3;
            s.dtcs[0] = 0x0300;  // P0300
            s.dtcs[1] = 0x0301;  // P0301
            s.dtcs[2] = 0x0304;  // P0304
        } else {
            // Período "melhor" mas não normal: 10-20s (DTCs permanecem!)
            eng.fault_toggle_ms = now + (uint32_t)frand(10000, 20000);
            // DTCs continuam ativos (MIL permanece aceso)
        }
    }

    // ── Máquina de estados urbana como base ──────────────────
    switch (eng.phase) {
        case PH_STOPPED:
            eng.actual_spd = approach(eng.actual_spd, 0, 0.5f);
            if (eng.fault_active) {
                // Idle instável com misfire
                float jitter = frand(-80.0f, 80.0f);
                eng.actual_rpm = ema(eng.actual_rpm, 750 + jitter, 0.12f);
            } else {
                eng.actual_rpm = ema(eng.actual_rpm, 780, 0.06f);
            }
            eng.actual_tps  = ema(eng.actual_tps, 4, 0.08f);
            eng.actual_load = ema(eng.actual_load, 22, 0.06f);
            if (now >= eng.phase_end_ms) {
                eng.phase      = PH_ACCEL;
                eng.target_spd = frand(25, 50);
            }
            break;

        case PH_ACCEL:
            eng.actual_spd = approach(eng.actual_spd, eng.target_spd, 0.7f);
            eng.actual_tps = ema(eng.actual_tps, 30, 0.10f);
            eng.actual_load= ema(eng.actual_load, 50, 0.08f);
            eng.actual_rpm = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.08f);
            if (eng.actual_spd >= eng.target_spd - 1.0f) {
                eng.phase       = PH_CRUISE;
                eng.phase_end_ms= now + (uint32_t)frand(6000, 18000);
            }
            break;

        case PH_CRUISE:
            eng.actual_spd = ema(eng.actual_spd, eng.target_spd, 0.03f);
            eng.actual_tps = ema(eng.actual_tps, 15, 0.06f);
            eng.actual_load= ema(eng.actual_load, 32, 0.04f);
            eng.actual_rpm = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.06f);
            if (now >= eng.phase_end_ms) {
                eng.phase       = PH_BRAKE;
                eng.phase_end_ms= now + (uint32_t)frand(4000, 7000);
            }
            break;

        case PH_BRAKE:
            eng.actual_spd = approach(eng.actual_spd, 0, 1.0f);
            eng.actual_tps = ema(eng.actual_tps, 0, 0.12f);
            eng.actual_load= ema(eng.actual_load, 8, 0.08f);
            eng.actual_rpm = ema(eng.actual_rpm, rpm_from_speed(eng.actual_spd), 0.08f);
            if (eng.actual_spd < 2.0f) {
                eng.actual_spd   = 0;
                eng.phase        = PH_STOPPED;
                eng.phase_end_ms = now + (uint32_t)frand(3000, 8000);
            }
            break;
    }

    // ── Efeitos da falha sobre os parâmetros ──────────────────
    if (eng.fault_active) {
        // RPM jitter irregular (misfire causa quedas momentâneas)
        float misfire_jitter = frand(-200.0f, 150.0f);
        if (frand(0, 1) < 0.15f) misfire_jitter = frand(-400.0f, -200.0f); // queda abrupta
        eng.actual_rpm += misfire_jitter * 0.1f;
        eng.actual_rpm = constrain(eng.actual_rpm, 500.0f, 5000.0f);

        // STFT alto: ECU adiciona combustível para compensar
        eng.stft_f = ema(eng.stft_f, 18.0f + frand(-3.0f, 3.0f), 0.05f);
        // LTFT sobe gradualmente
        eng.ltft_f = ema(eng.ltft_f, 8.0f, 0.005f);

        // Carga sobe (combustão ineficiente)
        eng.actual_load += 8.0f;
    } else {
        // Período "melhor": ainda não normal
        eng.stft_f = ema(eng.stft_f, 8.0f, 0.03f);
        eng.ltft_f = ema(eng.ltft_f, 6.0f, 0.003f);
    }

    eng.bat_f = ema(eng.bat_f, eng.fault_active ? 13.8f : 14.0f, 0.02f);
    eng.fuel_f -= 0.001f;  // consumo maior com misfire

    update_warm_engine_temps();
    write_state(s);
}

// ════════════════════════════════════════════════════════════
//  Task FreeRTOS — 10 Hz (100ms)
// ════════════════════════════════════════════════════════════

static void task_dynamic_engine(void*) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));

        xSemaphoreTake(s_mutex, portMAX_DELAY);

        SimMode mode = s_state->sim_mode;

        if (mode != SIM_STATIC) {
            // Detecta troca de modo
            if (mode != eng.prev_mode) {
                on_mode_enter(mode, *s_state);
                eng.prev_mode = mode;
            }

            // Atualiza canais de ruído suave
            update_noise();

            switch (mode) {
                case SIM_IDLE:    update_idle(*s_state);    break;
                case SIM_URBAN:   update_urban(*s_state);   break;
                case SIM_HIGHWAY: update_highway(*s_state); break;
                case SIM_WARMUP:  update_warmup(*s_state);  break;
                case SIM_FAULT:   update_fault(*s_state);   break;
                default: break;
            }
        } else {
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
