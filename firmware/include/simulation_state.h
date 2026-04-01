#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstring>

// ════════════════════════════════════════════════════════════
//  Modo de simulação dinâmica
// ════════════════════════════════════════════════════════════
enum SimMode : uint8_t {
    SIM_STATIC  = 0,  // manual — valores fixos controlados pelo utilizador
    SIM_IDLE    = 1,  // marcha lenta — motor aquecido, parado, oscilações leves
    SIM_URBAN   = 2,  // percurso urbano — stop & go, 0-60 km/h
    SIM_HIGHWAY = 3,  // rodovia — cruzeiro 100-120 km/h
    SIM_WARMUP  = 4,  // arranque a frio — temperatura sobe de 20 °C a 90 °C
    SIM_FAULT   = 5,  // falha intermitente — DTC P0300 cíclico + RPM errático
};
constexpr uint8_t SIM_MODE_COUNT = 6;

// ════════════════════════════════════════════════════════════
//  Estado central de simulação — 16 parâmetros + DTCs + VIN
//  Protegido por mutex FreeRTOS (g_sim_mutex)
// ════════════════════════════════════════════════════════════

struct SimulationState {
    // ── Parâmetros Mode 01 ────────────────────────────────────
    uint16_t rpm             = 800;    // 0–16000 RPM
    uint8_t  speed_kmh       = 0;      // 0–255 km/h
    int16_t  coolant_temp_c  = 90;     // -40 a +215 °C
    int16_t  intake_temp_c   = 30;     // -40 a +80 °C
    float    maf_gs          = 3.0f;   // 0.0–655.35 g/s
    uint8_t  map_kpa         = 35;     // 0–255 kPa
    uint8_t  throttle_pct    = 15;     // 0–100 %
    float    ignition_adv    = 10.0f;  // -64.0 a +63.5 °
    uint8_t  engine_load_pct = 25;     // 0–100 %
    uint8_t  fuel_level_pct  = 75;     // 0–100 %
    float    battery_voltage = 14.2f;  // 0.0–65.535 V
    int16_t  oil_temp_c      = 85;     // -40 a +210 °C
    float    stft_pct        = 0.0f;   // -100 a +99.2 % (Short Term Fuel Trim)
    float    ltft_pct        = 0.0f;   // -100 a +99.2 % (Long Term Fuel Trim)

    // ── DTCs Mode 03 ──────────────────────────────────────────
    uint8_t  dtc_count       = 0;
    uint16_t dtcs[8]         = {};     // até 8 DTCs (2 bytes cada)

    // ── VIN Mode 09 PID 02 ────────────────────────────────────
    char vin[18]             = "YOUSIM00000000001";

    // ── Protocolo ativo ───────────────────────────────────────
    uint8_t  active_protocol = 0;

    // ── Perfil de veículo ativo ───────────────────────────────
    char profile_id[24]      = "";     // vazio = nenhum perfil aplicado

    // ── Modo de simulação ─────────────────────────────────────
    SimMode  sim_mode        = SIM_STATIC;
};

// ── Presets de cenário ────────────────────────────────────────
namespace Preset {
    inline void applyOff(SimulationState& s) {
        s.rpm = 0; s.speed_kmh = 0; s.coolant_temp_c = 20;
        s.intake_temp_c = 20; s.maf_gs = 0; s.map_kpa = 100;
        s.throttle_pct = 0; s.ignition_adv = 0;
        s.engine_load_pct = 0; s.fuel_level_pct = 75;
        s.battery_voltage = 12.5f; s.oil_temp_c = 20;
        s.stft_pct = 0.0f; s.ltft_pct = 0.0f;
        s.dtc_count = 0; s.sim_mode = SIM_STATIC;
    }
    inline void applyIdle(SimulationState& s) {
        s.rpm = 800; s.speed_kmh = 0; s.coolant_temp_c = 90;
        s.intake_temp_c = 30; s.maf_gs = 3.0f; s.map_kpa = 35;
        s.throttle_pct = 15; s.ignition_adv = 10.0f;
        s.engine_load_pct = 20; s.fuel_level_pct = 75;
        s.battery_voltage = 14.2f; s.oil_temp_c = 85;
        s.stft_pct = 0.0f; s.ltft_pct = 0.0f;
        s.dtc_count = 0; s.sim_mode = SIM_STATIC;
    }
    inline void applyCruise(SimulationState& s) {
        s.rpm = 2000; s.speed_kmh = 60; s.coolant_temp_c = 92;
        s.intake_temp_c = 35; s.maf_gs = 12.0f; s.map_kpa = 55;
        s.throttle_pct = 35; s.ignition_adv = 15.0f;
        s.engine_load_pct = 45; s.fuel_level_pct = 60;
        s.battery_voltage = 14.0f; s.oil_temp_c = 90;
        s.stft_pct = -1.6f; s.ltft_pct = 0.0f;
        s.dtc_count = 0; s.sim_mode = SIM_STATIC;
    }
    inline void applyFullThrottle(SimulationState& s) {
        s.rpm = 5000; s.speed_kmh = 120; s.coolant_temp_c = 98;
        s.intake_temp_c = 45; s.maf_gs = 55.0f; s.map_kpa = 95;
        s.throttle_pct = 90; s.ignition_adv = 20.0f;
        s.engine_load_pct = 85; s.fuel_level_pct = 50;
        s.battery_voltage = 13.8f; s.oil_temp_c = 100;
        s.stft_pct = 4.7f; s.ltft_pct = 1.6f;
        s.dtc_count = 0; s.sim_mode = SIM_STATIC;
    }
    inline void applyOverheat(SimulationState& s) {
        applyIdle(s);
        s.coolant_temp_c = 115; s.oil_temp_c = 118;
        s.battery_voltage = 13.5f;
        s.dtc_count = 1; s.dtcs[0] = 0x0300;
    }
    inline void applyCatalystFail(SimulationState& s) {
        applyCruise(s);
        s.stft_pct = 12.5f; s.ltft_pct = 10.2f;
        s.dtc_count = 1; s.dtcs[0] = 0x0420;
    }
}
