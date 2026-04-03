#pragma once

#include <cstddef>
#include <cstdint>
#include "alert_types.h"

enum FaultId : uint8_t {
    FAULT_ID_NONE = 0,
    FAULT_ID_MISFIRE_MULTI = 1,
    FAULT_ID_MISFIRE_CYL_1 = 2,
    FAULT_ID_MISFIRE_CYL_4 = 3,
    FAULT_ID_FAN_DEGRADED = 4,
    FAULT_ID_COOLING_OVERTEMP = 5,
    FAULT_ID_AIR_LEAK_IDLE = 6,
    FAULT_ID_CATALYST_EFF_DROP = 7,
    FAULT_ID_VOLTAGE_UNSTABLE = 8,
    FAULT_ID_MODULE_SUPPLY_INSTABILITY = 9,
    FAULT_ID_ABS_WHEEL_SPEED_INCONSISTENT = 10,
};

struct FaultCatalogEntry {
    uint8_t fault_id;
    const char* slug;
    const char* label;
    DiagnosticSystem system;
    DiagnosticCompartment compartment;
    DiagnosticSeverity default_severity;
    uint16_t dtc;
    uint8_t emit_obd;
    FaultLifecycleStage qualify_stage;
    uint8_t base_penalty;
    uint8_t root_fault_id;
    uint8_t is_root_cause;
};

const FaultCatalogEntry* fault_catalog_get(uint8_t fault_id);
const FaultCatalogEntry* fault_catalog_all(size_t& count);

const char* diagnostic_system_slug(uint8_t system);
const char* diagnostic_system_label(uint8_t system);
const char* diagnostic_compartment_slug(uint8_t compartment);
const char* diagnostic_compartment_label(uint8_t compartment);
const char* diagnostic_severity_slug(uint8_t severity);
const char* diagnostic_severity_label(uint8_t severity);
const char* fault_stage_slug(uint8_t stage);
const char* fault_stage_label(uint8_t stage);
const char* diagnostic_alert_type_slug(uint8_t alert_type);
const char* diagnostic_alert_type_label(uint8_t alert_type);
