# modPowerElectronics

Power electronics control module - FETs, cell monitoring, safety.

## Overview

`modPowerElectronics` controls the power path (FETs), monitors cell voltages, and implements safety protection.

## Key Functions

### FET Control

```c
void modPowerElectronicsSetCharge(bool newState);
bool modPowerElectronicsSetDisCharge(bool newState);
void modPowerElectronicsSetPreCharge(bool newState);
void modPowerElectronicsDisableAll(void);
```

### Cell Monitoring

```c
void modPowerElectronicsCalculateCellStats(void);
// Populates: cellVoltageHigh, cellVoltageLow, cellVoltageAverage, cellVoltageMisMatch
```

### Temperature

```c
void modPowerElectronicsCalcTempStats(void);
// Populates: temperatures[], tempBatteryHigh, tempBatteryLow, tempBatteryAverage
```

### Balancing

```c
void modPowerElectronicsSubTaskBalaning(void);
// Uses: cellBalanceResistorEnableMask (bitmask of cells to balance)
```

## Data Structures

### Pack State

```c
typedef struct {
    float packVoltage;
    float packCurrent;
    float packPower;
    
    float cellVoltageHigh;
    float cellVoltageLow;
    float cellVoltageAverage;
    float cellVoltageMisMatch;
    
    uint8_t disChargeLCAllowed;
    uint8_t disChargeHCAllowed;
    uint8_t chargeAllowed;
    
    modPowerElectronicsPackOperationalCellStatesTypedef packOperationalCellState;
    
    float temperatures[NoOfTempSensors];
    float tempBatteryHigh;
    float tempBatteryLow;
} modPowerElectricsPackStateTypedef;
```

### Error States

```c
typedef enum {
    PACK_STATE_NORMAL,
    PACK_STATE_ERROR_HARD_CELLVOLTAGE,
    PACK_STATE_ERROR_SOFT_CELLVOLTAGE,
    PACK_STATE_ERROR_OVER_CURRENT
} modPowerElectronicsPackOperationalCellStatesTypedef;
```

## Hardware

- **Cell Monitor:** LTC6803-2 (12-cell)
- **Current Sense:** ISL28022 (I2C)
- **FETs:** Charge, Discharge, Pre-charge

## Dependencies

- `modConfig` — Configuration parameters
- `driverLTC6803` — Cell IC driver

## Related Documentation

- [Power Electronics](../systems/power-electronics.md)
- [Safety Checks](../systems/safety-checks.md)