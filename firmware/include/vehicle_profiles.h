#pragma once
#include "simulation_state.h"
#include "config.h"
#include <cstring>

// ════════════════════════════════════════════════════════════
//  Perfis de veículo — parâmetros OBD típicos em marcha lenta
//  Veículos populares no Brasil (2000–2024)
// ════════════════════════════════════════════════════════════

struct VehicleProfile {
    const char* id;          // identificador URL-safe, ex: "fiat_uno_10"
    const char* brand;       // "Fiat"
    const char* model;       // "Uno 1.0 Fire"
    const char* year;        // "2004-2013"
    uint8_t  protocol;       // PROTO_* de config.h
    uint16_t rpm;
    uint8_t  speed_kmh;
    int16_t  coolant_temp_c;
    int16_t  intake_temp_c;
    float    maf_gs;
    uint8_t  map_kpa;
    uint8_t  throttle_pct;
    float    ignition_adv;
    uint8_t  engine_load_pct;
    uint8_t  fuel_level_pct;
    const char* vin;         // 17 caracteres exatos
    uint8_t  supports_pid_a6_odometer = 0xFF; // 0xFF=auto, 0=false, 1=true
};

inline bool defaultPidA6SupportForProtocol(uint8_t protocol) {
    return protocol <= PROTO_CAN_29B_250K;
}

inline bool vehicleProfileSupportsPidA6(const VehicleProfile& p) {
    return p.supports_pid_a6_odometer == 0xFF
        ? defaultPidA6SupportForProtocol(p.protocol)
        : p.supports_pid_a6_odometer != 0;
}

inline bool vehicleProfileIsTurbo(const VehicleProfile& p) {
    return std::strstr(p.model, "Turbo") != nullptr ||
           std::strstr(p.model, "TSI") != nullptr ||
           std::strstr(p.model, "THP") != nullptr ||
           std::strstr(p.model, "TDI") != nullptr ||
           std::strstr(p.id, "_10t") != nullptr ||
           std::strstr(p.id, "_12t") != nullptr ||
           std::strstr(p.id, "_15t") != nullptr ||
           std::strstr(p.id, "_thp") != nullptr ||
           std::strstr(p.id, "_tdi") != nullptr;
}

// Aplica perfil ao estado (em marcha lenta, sem DTCs)
inline void applyVehicleProfile(SimulationState& s, const VehicleProfile& p) {
    s.active_protocol  = p.protocol;
    s.rpm              = p.rpm;
    s.speed_kmh        = p.speed_kmh;
    s.coolant_temp_c   = p.coolant_temp_c;
    s.intake_temp_c    = p.intake_temp_c;
    s.maf_gs           = p.maf_gs;
    s.map_kpa          = p.map_kpa;
    s.throttle_pct     = p.throttle_pct;
    s.ignition_adv     = p.ignition_adv;
    s.engine_load_pct  = p.engine_load_pct;
    s.fuel_level_pct   = p.fuel_level_pct;
    strncpy(s.vin, p.vin, 17);
    s.vin[17] = '\0';
    strncpy(s.profile_id, p.id, 23);
    s.profile_id[23] = '\0';
    simulation_clear_manual_dtcs(s);
    simulation_clear_effective_dtcs(s);
    s.distance_since_clear_km = 0;
    s.distance_mil_on_km = 0;
    s.odometer_source = ODOMETER_SOURCE_CLUSTER;
    s.pid_a6_supported = vehicleProfileSupportsPidA6(p) ? 1 : 0;
}

// ── Catálogo de veículos ──────────────────────────────────────
// { id, brand, model, year, proto, rpm, spd, cool, iat, maf, map, tps, ign, load, fuel, vin }
static const VehicleProfile VEHICLE_PROFILES[] = {
    // ── Fiat ─────────────────────────────────────────────────
    { "fiat_uno_10",
      "Fiat", "Uno 1.0 Fire", "2004-2013",
      PROTO_ISO9141,
      850, 0, 88, 28, 2.5f, 32, 12, 12.0f, 18, 65,
      "ZFA19800002345678" },

    { "fiat_palio_14",
      "Fiat", "Palio 1.4 Evo", "2013-2017",
      PROTO_CAN_11B_500K,
      800, 0, 90, 30, 3.0f, 35, 14, 11.0f, 20, 70,
      "ZFA19900003456789" },

    { "fiat_strada_14",
      "Fiat", "Strada 1.4", "2014-2020",
      PROTO_CAN_11B_500K,
      820, 0, 89, 31, 3.2f, 34, 13, 11.5f, 21, 60,
      "ZFA25500004567890" },

    { "fiat_argo_13",
      "Fiat", "Argo 1.3", "2017-2024",
      PROTO_CAN_11B_500K,
      780, 0, 91, 29, 3.8f, 36, 14, 12.5f, 22, 72,
      "ZFA33500005678901" },

    { "fiat_cronos_13",
      "Fiat", "Cronos 1.3", "2018-2024",
      PROTO_CAN_11B_500K,
      760, 0, 90, 30, 3.6f, 35, 13, 12.0f, 21, 68,
      "ZFA36600006789012" },

    // ── Volkswagen ────────────────────────────────────────────
    { "vw_gol_10",
      "Volkswagen", "Gol 1.0 MPI", "2012-2019",
      PROTO_CAN_11B_500K,
      700, 0, 87, 27, 2.8f, 30, 10, 13.0f, 17, 55,
      "9BWZZZ377VT004251" },

    { "vw_polo_16",
      "Volkswagen", "Polo 1.6 MSI", "2018-2022",
      PROTO_CAN_11B_500K,
      750, 0, 91, 29, 4.5f, 38, 14, 14.5f, 22, 72,
      "9BWAG05J014016753" },

    { "vw_virtus_16",
      "Volkswagen", "Virtus 1.6 MSI", "2018-2024",
      PROTO_CAN_11B_500K,
      720, 0, 90, 28, 4.2f, 37, 13, 14.0f, 21, 68,
      "9BWFB45V0JT000123" },

    { "vw_t_cross_15t",
      "Volkswagen", "T-Cross 1.4 TSI", "2019-2024",
      PROTO_CAN_11B_500K,
      680, 0, 92, 28, 5.5f, 40, 15, 15.5f, 24, 75,
      "9BWCA05J0KT001234" },

    // ── Chevrolet ─────────────────────────────────────────────
    { "gm_celta_10",
      "Chevrolet", "Celta 1.0 VHC-E", "2006-2015",
      PROTO_KWP_FAST,
      900, 0, 86, 32, 2.2f, 33, 11, 10.5f, 19, 50,
      "9BGRD08X04G123456" },

    { "gm_onix_10t",
      "Chevrolet", "Onix 1.0 Turbo", "2020-2024",
      PROTO_CAN_11B_500K,
      650, 0, 93, 28, 6.0f, 42, 16, 16.0f, 25, 80,
      "9BGRU6550L1234567" },

    { "gm_tracker_12t",
      "Chevrolet", "Tracker 1.2T", "2021-2024",
      PROTO_CAN_11B_500K,
      680, 0, 94, 30, 7.0f, 45, 15, 17.0f, 28, 75,
      "9BGEC2E82M5123456" },

    // ── Ford ──────────────────────────────────────────────────
    { "ford_ka_10",
      "Ford", "Ka 1.0 SE Plus", "2015-2020",
      PROTO_CAN_11B_500K,
      780, 0, 90, 29, 3.5f, 36, 13, 12.5f, 20, 65,
      "9BFBA10G0H1234567" },

    { "ford_ecosport_15",
      "Ford", "EcoSport 1.5", "2013-2022",
      PROTO_CAN_11B_500K,
      820, 0, 91, 30, 5.2f, 40, 14, 13.5f, 23, 60,
      "9FBSA4HT0HBB12345" },

    // ── Toyota ────────────────────────────────────────────────
    { "toyota_corolla_20",
      "Toyota", "Corolla 2.0 XEi", "2020-2024",
      PROTO_CAN_11B_500K,
      650, 0, 93, 30, 5.0f, 40, 15, 15.5f, 23, 75,
      "9BRBF69H0L1234567" },

    { "toyota_hilux_28",
      "Toyota", "Hilux 2.8 TDI", "2016-2024",
      PROTO_CAN_11B_500K,
      700, 0, 88, 35, 8.5f, 50, 12, 8.0f, 22, 55,
      "9BRFC18J0H0123456" },

    // ── Honda ─────────────────────────────────────────────────
    { "honda_civic_20",
      "Honda", "Civic 2.0 EXL", "2017-2021",
      PROTO_CAN_11B_500K,
      700, 0, 92, 30, 4.8f, 38, 14, 14.0f, 22, 68,
      "2HGFB2F5XCH123456" },

    { "honda_hrv_15",
      "Honda", "HR-V 1.5 EX", "2015-2021",
      PROTO_CAN_11B_500K,
      720, 0, 91, 29, 4.2f, 37, 13, 13.5f, 21, 70,
      "2HKRM4H50GH123456" },

    // ── Hyundai ───────────────────────────────────────────────
    { "hyundai_hb20_10",
      "Hyundai", "HB20 1.0 Sense", "2020-2024",
      PROTO_CAN_11B_500K,
      750, 0, 90, 28, 3.2f, 34, 12, 13.5f, 20, 62,
      "9BHBM51APKT123456" },

    { "hyundai_creta_16",
      "Hyundai", "Creta 1.6 Pulse", "2017-2021",
      PROTO_CAN_11B_500K,
      730, 0, 91, 29, 4.8f, 39, 14, 14.0f, 23, 70,
      "9BHDA51JL0T123456" },

    // ── Renault ───────────────────────────────────────────────
    { "peugeot_308_16thp",
      "Peugeot", "308 1.6 THP", "2013-2019",
      PROTO_CAN_11B_500K,
      720, 0, 93, 31, 5.8f, 41, 15, 11.5f, 24, 70,
      "8AD4C5FVAFG123456" },

    { "renault_kwid_10",
      "Renault", "Kwid 1.0 Zen", "2017-2024",
      PROTO_CAN_11B_500K,
      850, 0, 88, 30, 2.8f, 32, 11, 12.0f, 18, 58,
      "VF1AGA000H0123456" },

    { "renault_sandero_16",
      "Renault", "Sandero 1.6 Stepway", "2015-2020",
      PROTO_CAN_11B_500K,
      800, 0, 89, 29, 4.0f, 36, 13, 13.0f, 21, 65,
      "VF1BSF20H39123456" },
};

constexpr uint8_t VEHICLE_PROFILE_COUNT =
    sizeof(VEHICLE_PROFILES) / sizeof(VEHICLE_PROFILES[0]);

// Busca perfil por ID — retorna nullptr se não encontrar
inline const VehicleProfile* findProfile(const char* id) {
    for (uint8_t i = 0; i < VEHICLE_PROFILE_COUNT; i++) {
        if (!strcmp(VEHICLE_PROFILES[i].id, id)) return &VEHICLE_PROFILES[i];
    }
    return nullptr;
}

inline const VehicleProfile* activeVehicleProfile(const SimulationState& s) {
    return s.profile_id[0] == '\0' ? nullptr : findProfile(s.profile_id);
}

inline bool activeVehicleProfileIsTurbo(const SimulationState& s) {
    const VehicleProfile* profile = activeVehicleProfile(s);
    return profile != nullptr && vehicleProfileIsTurbo(*profile);
}
