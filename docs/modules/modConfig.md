# modConfig

Configuration management module - flash-based parameter storage.

## Overview

`modConfig` manages non-volatile configuration storage using flash EEPROM emulation.

## Key Functions

### Initialization

```c
void modConfigInit(void);
```

Loads config from flash or defaults.

### Get Configuration

```c
modConfigGeneralConfigStructTypedef* modConfigGet(void);
// Returns pointer to general config
```

### Save Configuration

```c
void modConfigSave(void);
// Saves config to flash with CRC
```

### Reset to Defaults

```c
void modConfigReset(void);
// Resets all config to default values
```

## Configuration Structure

```c
typedef struct {
    // Voltage limits
    float maxCellVoltageHard;
    float maxCellVoltageSoft;
    float minCellVoltageHard;
    float minCellVoltageSoft;
    
    // Current limits
    float maxDischargeCurrentHard;
    float maxDischargeCurrentSoft;
    float maxChargeCurrentHard;
    float maxChargeCurrentSoft;
    
    // Temperature limits
    float maxBatteryTempHard;
    float maxBatteryTempSoft;
    float minBatteryTemp;
    
    // Timing
    uint32_t timeoutLCPreCharge;
    uint32_t notUsedTimeout;
    
    // Features
    uint8_t allowChargingDuringDischarge;
    uint8_t balancingEnabled;
} modConfigGeneralConfigStructTypedef;
```

## Storage

- **Location:** Flash memory (EEPROM emulation)
- **Size:** 2KB
- **Protection:** CRC checksum validation
- **Backup:** Redundant storage

## Terminal Commands

```
config show              // Display all config
config get <param>       // Get single parameter
config set <param> <val> // Set parameter
config save              // Save to flash
config reset             // Reset to defaults
```

## Related Documentation

- [Configuration](../systems/configuration.md)