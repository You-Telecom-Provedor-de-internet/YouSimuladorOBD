#pragma once

#include <cstdint>
#include <type_traits>

enum DiagnosticSystem : uint8_t {
    DIAG_SYS_NONE = 0,
    DIAG_SYS_ENGINE = 1,
    DIAG_SYS_COOLING = 2,
    DIAG_SYS_INTAKE = 3,
    DIAG_SYS_EXHAUST = 4,
    DIAG_SYS_IGNITION = 5,
    DIAG_SYS_FUEL = 6,
    DIAG_SYS_BATTERY_CHARGE = 7,
    DIAG_SYS_BRAKES_ABS = 8,
    DIAG_SYS_TRANSMISSION = 9,
    DIAG_SYS_STEERING = 10,
    DIAG_SYS_SUSPENSION = 11,
    DIAG_SYS_EMISSIONS = 12,
    DIAG_SYS_ELECTRONICS_PANEL = 13,
};

enum DiagnosticCompartment : uint8_t {
    DIAG_COMP_NONE = 0,
    DIAG_COMP_POWERTRAIN = 1,
    DIAG_COMP_ENGINE_BAY = 2,
    DIAG_COMP_COOLING_LOOP = 3,
    DIAG_COMP_INTAKE_PATH = 4,
    DIAG_COMP_EXHAUST_LINE = 5,
    DIAG_COMP_FUEL_SYSTEM = 6,
    DIAG_COMP_ELECTRICAL_BUS = 7,
    DIAG_COMP_CHASSIS = 8,
    DIAG_COMP_FRONT_AXLE = 9,
    DIAG_COMP_REAR_AXLE = 10,
    DIAG_COMP_CABIN_CLUSTER = 11,
};

enum DiagnosticSeverity : uint8_t {
    DIAG_SEV_INFO = 0,
    DIAG_SEV_WARNING = 1,
    DIAG_SEV_HIGH = 2,
    DIAG_SEV_CRITICAL = 3,
};

enum FaultLifecycleStage : uint8_t {
    FAULT_STAGE_NONE = 0,
    FAULT_STAGE_ONSET = 1,
    FAULT_STAGE_AGGRAVATING = 2,
    FAULT_STAGE_INTERMITTENT = 3,
    FAULT_STAGE_PERSISTENT = 4,
    FAULT_STAGE_RECOVERING = 5,
};

enum DiagnosticAlertType : uint8_t {
    DIAG_ALERT_NONE = 0,
    DIAG_ALERT_PERFORMANCE_LOSS = 1,
    DIAG_ALERT_COOLING_CRITICAL = 2,
    DIAG_ALERT_LEAN_CONDITION = 3,
    DIAG_ALERT_EMISSIONS_DEGRADED = 4,
    DIAG_ALERT_VOLTAGE_UNSTABLE = 5,
    DIAG_ALERT_ABS_INCONSISTENT = 6,
    DIAG_ALERT_LIMP_MODE_ACTIVE = 7,
    DIAG_ALERT_IGNITION_STABILITY = 8,
    DIAG_ALERT_ELECTRONICS_INSTABILITY = 9,
};

struct ActiveFault {
    uint16_t dtc = 0;
    uint8_t  fault_id = 0;
    uint8_t  system = DIAG_SYS_NONE;
    uint8_t  compartment = DIAG_COMP_NONE;
    uint8_t  severity = DIAG_SEV_INFO;
    uint8_t  stage = FAULT_STAGE_NONE;
    uint8_t  intensity_pct = 0;
};

struct DiagnosticAlert {
    uint8_t type = DIAG_ALERT_NONE;
    uint8_t severity = DIAG_SEV_INFO;
    uint8_t system = DIAG_SYS_NONE;
    uint8_t compartment = DIAG_COMP_NONE;
    uint8_t fault_id = 0;
    uint8_t flags = 0;
};

static_assert(sizeof(ActiveFault) == 8, "ActiveFault must remain compact");
static_assert(sizeof(DiagnosticAlert) == 6, "DiagnosticAlert must remain compact");
static_assert(std::is_trivially_copyable<ActiveFault>::value, "ActiveFault must be POD-like");
static_assert(std::is_trivially_copyable<DiagnosticAlert>::value, "DiagnosticAlert must be POD-like");
