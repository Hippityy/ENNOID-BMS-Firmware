# Configuration System

The BMS uses a flash-based configuration system that stores parameters in non-volatile memory.

## Overview

- **Storage:** Flash memory (EEPROM emulation)
- **Default Values:** Compiled-in defaults on first boot
- **Backup:** Redundant storage for safety
- **Validation:** CRC checksum verification

## Configuration Structure

```c
typedef struct {
    // General settings
    modConfigGeneralConfigStructTypedef general;
    
    // CAN settings
    modConfigCANConfigTypedef can;
    
    // Display settings
    modConfigDisplayConfigTypedef display;
    
    // System
    uint32_t crc;           // CRC checksum
    uint32_t version;       // Config version
} modConfigStructTypedef;
```

## General Configuration

The general configuration contains all safety and operational parameters:

```c
typedef struct {
    // === Voltage Limits ===
    float maxCellVoltageHard;        // Hard over-voltage (V)
    float maxCellVoltageSoft;        // Soft over-voltage (V)
    float minCellVoltageHard;        // Hard under-voltage (V)
    float minCellVoltageSoft;        // Soft under-voltage (V)
    
    // === Current Limits ===
    float maxDischargeCurrentHard;   // Max discharge (A)
    float maxDischargeCurrentSoft;   // Soft discharge limit (A)
    float maxChargeCurrentHard;      // Max charge (A)
    float maxChargeCurrentSoft;      // Soft charge limit (A)
    
    // === Temperature Limits ===
    float maxBatteryTempHard;        // Hard temp limit (°C)
    float maxBatteryTempSoft;        // Soft temp limit (°C)
    float minBatteryTemp;            // Minimum temp (°C)
    
    // === Timing ===
    uint32_t timeoutLCPreCharge;     // Pre-charge timeout (ms)
    uint32_t timeoutChargingComplete; // Charge complete timeout (ms)
    uint32_t notUsedTimeout;         // Idle timeout (ms)
    
    // === Thresholds ===
    float minimalPrechargePercentage; // Pre-charge complete (%)
    float maxMismatchThreshold;      // Cell imbalance (V)
    float notUsedCurrentThreshold;   // "In use" current (A)
    
    // === Feature Flags ===
    uint8_t allowChargingDuringDischarge; // Allow charger while load connected
    uint8_t balancingEnabled;        // Cell balancing enable
    
    // === Timeouts ===
    uint32_t displayTimeoutSplashScreen; // Splash screen time (ms)
    uint32_t timeoutChargingCompletedMinimalMismatch; // Balance timeout (ms)
} modConfigGeneralConfigStructTypedef;
```

## Default Values

| Parameter | Default | Description |
|-----------|---------|-------------|
| maxCellVoltageHard | 4.20V | Hard OV limit |
| maxCellVoltageSoft | 4.15V | Soft OV limit |
| minCellVoltageHard | 2.50V | Hard UV limit |
| minCellVoltageSoft | 2.80V | Soft UV limit |
| maxDischargeCurrentHard | 200A | Max discharge |
| maxDischargeCurrentSoft | 160A | Soft discharge limit |
| maxChargeCurrentHard | 100A | Max charge |
| maxChargeCurrentSoft | 80A | Soft charge limit |
| maxBatteryTempHard | 55°C | Hard temp limit |
| maxBatteryTempSoft | 45°C | Soft temp limit |
| timeoutLCPreCharge | 5000ms | Pre-charge timeout |
| minimalPrechargePercentage | 75% | Pre-charge complete |
| maxMismatchThreshold | 0.05V | Cell imbalance limit |

## Configuration Access

### Via Terminal

The terminal interface provides configuration commands:

```
> config show              // Show all config
> config get maxCellVoltageHard
> config set maxCellVoltageHard 4.15
> config save              // Save to flash
> config reset             // Reset to defaults
```

### Via CAN

Configuration can also be accessed via CAN messages.

### Via Code

```c
// Get pointer to config
modConfigGeneralConfigStructTypedef* config = modConfigGet();

// Read a value
float maxVoltage = config->maxCellVoltageHard;

// Modify a value
config->maxCellVoltageHard = 4.20f;

// Save to flash
modConfigSave();
```

## Storage Details

### Flash Layout

```
Flash Address:
0x0803E000 - 0x0803FFFF  (8KB)
```

### Data Structure

```c
typedef struct {
    uint32_t header;           // Magic number (0xDEADBEEF)
    modConfigStructTypedef data;
    uint32_t crc;             // CRC32 of data
    uint32_t backupCrc;       // Backup CRC
} modConfigFlashTypedef;
```

### Write Process

1. Read current config from flash
2. Validate CRC
3. If invalid, load defaults
4. Modify in RAM
5. Calculate new CRC
6. Write to primary location
7. Verify write
8. Update backup

## Version Management

Config version allows migration between firmware versions:

- If config version < firmware version: migrate config
- If config version > firmware version: reset to defaults (incompatible)

## Related Documentation

- [Safety Checks](safety-checks.md) — Safety parameter details
- [modConfig](../modules/modConfig.md) — Module reference
- [Getting Started](../getting-started/troubleshooting.md) — Config issues