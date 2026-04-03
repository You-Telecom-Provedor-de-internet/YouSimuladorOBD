#include "fault_catalog.h"
#include <cstring>

static const FaultCatalogEntry FAULT_CATALOG[] = {
    { FAULT_ID_NONE, "", "", DIAG_SYS_NONE, DIAG_COMP_NONE, DIAG_SEV_INFO, 0x0000, 0, FAULT_STAGE_NONE, 0, FAULT_ID_NONE, 0 },
    { FAULT_ID_MISFIRE_MULTI, "misfire_multi", "Misfire progressivo", DIAG_SYS_IGNITION, DIAG_COMP_POWERTRAIN, DIAG_SEV_HIGH, 0x0300, 1, FAULT_STAGE_AGGRAVATING, 28, FAULT_ID_MISFIRE_MULTI, 1 },
    { FAULT_ID_MISFIRE_CYL_1, "misfire_cyl_1", "Misfire cil 1", DIAG_SYS_ENGINE, DIAG_COMP_POWERTRAIN, DIAG_SEV_HIGH, 0x0301, 1, FAULT_STAGE_INTERMITTENT, 11, FAULT_ID_MISFIRE_MULTI, 0 },
    { FAULT_ID_MISFIRE_CYL_4, "misfire_cyl_4", "Misfire cil 4", DIAG_SYS_ENGINE, DIAG_COMP_POWERTRAIN, DIAG_SEV_HIGH, 0x0304, 1, FAULT_STAGE_INTERMITTENT, 11, FAULT_ID_MISFIRE_MULTI, 0 },
    { FAULT_ID_FAN_DEGRADED, "fan_degraded", "Ventoinha fraca", DIAG_SYS_COOLING, DIAG_COMP_COOLING_LOOP, DIAG_SEV_WARNING, 0x0000, 0, FAULT_STAGE_ONSET, 12, FAULT_ID_FAN_DEGRADED, 1 },
    { FAULT_ID_COOLING_OVERTEMP, "cooling_overtemp", "Superaquecimento", DIAG_SYS_COOLING, DIAG_COMP_ENGINE_BAY, DIAG_SEV_CRITICAL, 0x0217, 1, FAULT_STAGE_PERSISTENT, 35, FAULT_ID_FAN_DEGRADED, 0 },
    { FAULT_ID_AIR_LEAK_IDLE, "air_leak_idle", "Falso ar", DIAG_SYS_INTAKE, DIAG_COMP_INTAKE_PATH, DIAG_SEV_HIGH, 0x0171, 1, FAULT_STAGE_AGGRAVATING, 24, FAULT_ID_AIR_LEAK_IDLE, 1 },
    { FAULT_ID_CATALYST_EFF_DROP, "catalyst_eff_drop", "Catalisador ineficiente", DIAG_SYS_EMISSIONS, DIAG_COMP_EXHAUST_LINE, DIAG_SEV_WARNING, 0x0420, 1, FAULT_STAGE_PERSISTENT, 18, FAULT_ID_CATALYST_EFF_DROP, 1 },
    { FAULT_ID_VOLTAGE_UNSTABLE, "voltage_unstable", "Tensao instavel", DIAG_SYS_BATTERY_CHARGE, DIAG_COMP_ELECTRICAL_BUS, DIAG_SEV_HIGH, 0x0562, 1, FAULT_STAGE_PERSISTENT, 26, FAULT_ID_VOLTAGE_UNSTABLE, 1 },
    { FAULT_ID_MODULE_SUPPLY_INSTABILITY, "module_supply_instability", "Modulos instaveis", DIAG_SYS_ELECTRONICS_PANEL, DIAG_COMP_CABIN_CLUSTER, DIAG_SEV_WARNING, 0x0000, 0, FAULT_STAGE_AGGRAVATING, 10, FAULT_ID_VOLTAGE_UNSTABLE, 0 },
    { FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT, "abs_wheel_speed_inconsistent", "Roda ABS inconsistente", DIAG_SYS_BRAKES_ABS, DIAG_COMP_CHASSIS, DIAG_SEV_WARNING, 0x0000, 0, FAULT_STAGE_INTERMITTENT, 12, FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT, 1 },
};

static const char* SYSTEM_SLUGS[] = {
    "none",
    "motor",
    "arrefecimento",
    "admissao",
    "escape",
    "ignicao",
    "combustivel",
    "bateria_carga",
    "freios_abs",
    "transmissao",
    "direcao",
    "suspensao",
    "emissoes",
    "eletronica_painel",
};

static const char* SYSTEM_LABELS[] = {
    "Sem sistema",
    "Motor",
    "Arrefecimento",
    "Admissao",
    "Escape",
    "Ignicao",
    "Combustivel",
    "Bateria/Carga",
    "Freios/ABS",
    "Transmissao",
    "Direcao",
    "Suspensao",
    "Emissoes",
    "Eletronica/Painel",
};

static const char* COMPARTMENT_SLUGS[] = {
    "none",
    "powertrain",
    "engine_bay",
    "cooling_loop",
    "intake_path",
    "exhaust_line",
    "fuel_system",
    "electrical_bus",
    "chassis",
    "front_axle",
    "rear_axle",
    "cabin_cluster",
};

static const char* COMPARTMENT_LABELS[] = {
    "Sem comp.",
    "Powertrain",
    "Cofre do motor",
    "Arrefecimento",
    "Admissao",
    "Linha de escape",
    "Combustivel",
    "Barr. eletrico",
    "Chassi",
    "Eixo dianteiro",
    "Eixo traseiro",
    "Cabine",
};

static const char* SEVERITY_SLUGS[] = { "info", "warning", "high", "critical" };
static const char* SEVERITY_LABELS[] = { "Info", "Warning", "Alta", "Critica" };

static const char* STAGE_SLUGS[] = { "none", "onset", "aggravating", "intermittent", "persistent", "recovering" };
static const char* STAGE_LABELS[] = { "None", "Inicio", "Agrav.", "Intermit.", "Persist.", "Recup." };

static const char* ALERT_SLUGS[] = {
    "none",
    "performance_loss",
    "cooling_critical",
    "lean_condition",
    "emissions_degraded",
    "voltage_unstable",
    "abs_inconsistent",
    "limp_mode_active",
    "ignition_stability",
    "electronics_instability",
};

static const char* ALERT_LABELS[] = {
    "Sem alerta",
    "Perda desempenho",
    "Arref. critico",
    "Mistura pobre",
    "Emissoes degradadas",
    "Tensao instavel",
    "ABS inconsistente",
    "Limp ativo",
    "Ignicao instavel",
    "Eletronica instavel",
};

const FaultCatalogEntry* fault_catalog_get(uint8_t fault_id) {
    for (size_t i = 0; i < sizeof(FAULT_CATALOG) / sizeof(FAULT_CATALOG[0]); i++) {
        if (FAULT_CATALOG[i].fault_id == fault_id) {
            return &FAULT_CATALOG[i];
        }
    }
    return &FAULT_CATALOG[0];
}

const FaultCatalogEntry* fault_catalog_all(size_t& count) {
    count = sizeof(FAULT_CATALOG) / sizeof(FAULT_CATALOG[0]);
    return FAULT_CATALOG;
}

static const char* safe_lookup(const char* const* table, size_t count, uint8_t idx) {
    return idx < count ? table[idx] : table[0];
}

const char* diagnostic_system_slug(uint8_t system) {
    return safe_lookup(SYSTEM_SLUGS, sizeof(SYSTEM_SLUGS) / sizeof(SYSTEM_SLUGS[0]), system);
}

const char* diagnostic_system_label(uint8_t system) {
    return safe_lookup(SYSTEM_LABELS, sizeof(SYSTEM_LABELS) / sizeof(SYSTEM_LABELS[0]), system);
}

const char* diagnostic_compartment_slug(uint8_t compartment) {
    return safe_lookup(COMPARTMENT_SLUGS, sizeof(COMPARTMENT_SLUGS) / sizeof(COMPARTMENT_SLUGS[0]), compartment);
}

const char* diagnostic_compartment_label(uint8_t compartment) {
    return safe_lookup(COMPARTMENT_LABELS, sizeof(COMPARTMENT_LABELS) / sizeof(COMPARTMENT_LABELS[0]), compartment);
}

const char* diagnostic_severity_slug(uint8_t severity) {
    return safe_lookup(SEVERITY_SLUGS, sizeof(SEVERITY_SLUGS) / sizeof(SEVERITY_SLUGS[0]), severity);
}

const char* diagnostic_severity_label(uint8_t severity) {
    return safe_lookup(SEVERITY_LABELS, sizeof(SEVERITY_LABELS) / sizeof(SEVERITY_LABELS[0]), severity);
}

const char* fault_stage_slug(uint8_t stage) {
    return safe_lookup(STAGE_SLUGS, sizeof(STAGE_SLUGS) / sizeof(STAGE_SLUGS[0]), stage);
}

const char* fault_stage_label(uint8_t stage) {
    return safe_lookup(STAGE_LABELS, sizeof(STAGE_LABELS) / sizeof(STAGE_LABELS[0]), stage);
}

const char* diagnostic_alert_type_slug(uint8_t alert_type) {
    return safe_lookup(ALERT_SLUGS, sizeof(ALERT_SLUGS) / sizeof(ALERT_SLUGS[0]), alert_type);
}

const char* diagnostic_alert_type_label(uint8_t alert_type) {
    return safe_lookup(ALERT_LABELS, sizeof(ALERT_LABELS) / sizeof(ALERT_LABELS[0]), alert_type);
}
