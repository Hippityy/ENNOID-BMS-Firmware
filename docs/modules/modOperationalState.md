# modOperationalState

State machine module that controls BMS operational modes.

## Overview

`modOperationalState` manages the operational state of the BMS, handling transitions between different modes like initialization, charging, discharging, and error states.

## Key Functions

### Initialization

```c
void modOperationalStateInit(
    modPowerElectricsPackStateTypedef *packState,
    modConfigGeneralConfigStructTypedef *generalConfigPointer,
    modStateOfChargeStructTypeDef *generalStateOfCharge
);
```

### Main Task

```c
void modOperationalStateTask(void);
```

Called every 10ms in the main loop. Handles state machine logic.

### State Control

```c
void modOperationalStateSetNewState(OperationalStateTypedef newState);
void modOperationalStateSetAllStates(OperationalStateTypedef newState);
```

### Utility

```c
void modOperationalStateTerminateOperation(void);
bool modOperationalStateDelayedDisable(bool delayedPowerDownDesired);
```

## States

| State | Value | Description |
|-------|-------|-------------|
| `OP_STATE_INIT` | 0 | Initialization |
| `OP_STATE_CHARGING` | 1 | Charging mode |
| `OP_STATE_PRE_CHARGE` | 2 | Pre-charge sequence |
| `OP_STATE_LOAD_ENABLED` | 3 | Load powered |
| `OP_STATE_FORCEON` | 4 | Forced on (bypass) |
| `OP_STATE_BALANCING` | 5 | Cell balancing |
| `OP_STATE_POWER_DOWN` | 6 | Safe shutdown |
| `OP_STATE_BATTERY_DEAD` | 7 | Battery too low |
| `OP_STATE_ERROR_PRECHARGE` | 8 | Pre-charge error |

## Configuration

Key configuration parameters:

- `displayTimeoutSplashScreen` — Splash screen duration
- `timeoutLCPreCharge` — Pre-charge timeout
- `minimalPrechargePercentage` — Pre-charge complete threshold
- `notUsedTimeout` — Idle timeout
- `allowChargingDuringDischarge` — Allow simultaneous charge/discharge

## Dependencies

- `modPowerElectronics` — Pack state, FET control
- `modConfig` — Configuration parameters
- `modStateOfCharge` — SOC information
- `modDisplay` — UI updates

## Related Documentation

- [State Machine](../systems/state-machine.md)
- [Safety Checks](../systems/safety-checks.md)