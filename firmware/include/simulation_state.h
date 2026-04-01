#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstring>

// ════════════════════════════════════════════════════════════
//  Estado central de simulação — os 12 parâmetros + DTCs + VIN
//  Protegido por mutex FreeRTOS (SimulationManager)
// ════════════════════════════════════════════════════════════

struct SimulationState {
    // ── Parâmetros Mode 01 ────────────────────────────────────
    uint16_t rpm             = 800;    // 0–16000 RPM
    uint8_t  speed_kmh       = 0;     // 0–255 km/h
    int16_t  coolant_temp_c  = 90;    // -40 a +215 °C
    int16_t  intake_temp_c   = 30;    // -40 a +80 °C
    float    maf_gs          = 3.0f;  // 0.0–655.35 g/s
    uint8_t  map_kpa         = 35;    // 0–255 kPa
    uint8_t  throttle_pct    = 15;    // 0–100 %
    float    ignition_adv    = 10.0f; // -64.0 a +63.5 °
    uint8_t  engine_load_pct = 25;    // 0–100 %
    uint8_t  fuel_level_pct  = 75;    // 0–100 %

    // ── DTCs Mode 03 ──────────────────────────────────────────
    uint8_t  dtc_count       = 0;
    uint16_t dtcs[8]         = {};    // até 8 DTCs (2 bytes cada)

    // ── VIN Mode 09 PID 02 ────────────────────────────────────
    char vin[18]             = "YOUSIM00000000001";

    // ── Protocolo ativo ───────────────────────────────────────
    uint8_t  active_protocol = 0;

    // ── Perfil de veículo ativo ───────────────────────────────
    char profile_id[24]      = "";   // vazio = nenhum perfil aplicado
};

// ── Presets de cenário ────────────────────────────────────────
namespace Preset {
    inline void applyOff(SimulationState& s) {
        s.rpm = 0; s.speed_kmh = 0; s.coolant_temp_c = 20;
        s.intake_temp_c = 20; s.maf_gs = 0; s.map_kpa = 100;
        s.throttle_pct = 0; s.ignition_adv = 0;
        s.engine_load_pct = 0; s.fuel_level_pct = 75;
        s.dtc_count = 0;
    }
    inline void applyIdle(SimulationState& s) {
        s.rpm = 800; s.speed_kmh = 0; s.coolant_temp_c = 90;
        s.intake_temp_c = 30; s.maf_gs = 3.0f; s.map_kpa = 35;
        s.throttle_pct = 15; s.ignition_adv = 10.0f;
        s.engine_load_pct = 20; s.fuel_level_pct = 75;
        s.dtc_count = 0;
    }
    inline void applyCruise(SimulationState& s) {
        s.rpm = 2000; s.speed_kmh = 60; s.coolant_temp_c = 92;
        s.intake_temp_c = 35; s.maf_gs = 12.0f; s.map_kpa = 55;
        s.throttle_pct = 35; s.ignition_adv = 15.0f;
        s.engine_load_pct = 45; s.fuel_level_pct = 60;
        s.dtc_count = 0;
    }
    inline void applyFullThrottle(SimulationState& s) {
        s.rpm = 5000; s.speed_kmh = 120; s.coolant_temp_c = 98;
        s.intake_temp_c = 45; s.maf_gs = 55.0f; s.map_kpa = 95;
        s.throttle_pct = 90; s.ignition_adv = 20.0f;
        s.engine_load_pct = 85; s.fuel_level_pct = 50;
        s.dtc_count = 0;
    }
    inline void applyOverheat(SimulationState& s) {
        applyIdle(s);
        s.coolant_temp_c = 115;
        s.dtc_count = 1; s.dtcs[0] = 0x0300; // P0300
    }
    inline void applyCatalystFail(SimulationState& s) {
        applyCruise(s);
        s.dtc_count = 1; s.dtcs[0] = 0x0420; // P0420
    }
}
