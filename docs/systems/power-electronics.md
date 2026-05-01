# Power Electronics

The power electronics module (`modPowerElectronics`) controls the FETs, monitors cells, and manages the power path.

## Overview

The power electronics system controls:
- **Charge FET** — Enables/disables charging
- **Discharge FET** — Enables/disables discharging
- **Pre-charge FET** — Soft-start for capacitive loads
- **Cell Balancing** — Equalize cell voltages

## Hardware

### FET Configuration

```
       +------------------+
       |                  |
 Battery+ ---+--[D]----+--[D]----+-- Load+
       |    |   |       |         |
       |   CHG   PRE    DIS       |
       |    |    |       |         |
       |    +----+-------+---------+
       |         |                 |
       |        GND               GND
       |
       +------------------+
```

**FETs:**
- **CHG (Charge)** — Controls charging current flow
- **DIS (Discharge)** — Controls discharge current flow
- **PRE (Pre-charge)** — Limits inrush current to capacitive loads

### Cell Monitoring

**LTC6803-2 IC:**
- 12-cell voltage monitoring
- Isolated communication (SPI)
- Passive cell balancing outputs

**Sensors:**
- Cell voltages (12 channels)
- Internal temperature (chip)
- External NTC thermistors

### Current Sensing

**ISL28022:**
- High-side current sense amplifier
- I2C interface
- Programmable gain

## Module Functions

### FET Control

```c
// Set charge FET state
void modPowerElectronicsSetCharge(bool newState);

// Set discharge FET state
bool modPowerElectronicsSetDisCharge(bool newState);

// Set pre-charge FET state
void modPowerElectronicsSetPreCharge(bool newState);

// Disable all FETs
void modPowerElectronicsDisableAll(void);
```

### Cell Monitoring

```c
// Read all cell voltages
void modPowerElectronicsCalculateCellStats(void);

// Get cell statistics
float cellVoltageHigh;   // Highest cell
float cellVoltageLow;    // Lowest cell
float cellVoltageAverage; // Average
float cellVoltageMisMatch; // Difference (high - low)
```

### Balancing

```c
// Run balancing sub-task
void modPowerElectronicsSubTaskBalaning(void);

// Enable/disable cell balancing
uint16_t cellBalanceResistorEnableMask;  // Bitmask of cells to balance
```

### Temperature

```c
// Calculate temperature statistics
void modPowerElectronicsCalcTempStats(void);

// Temperature values
float temperatures[NoOfTempSensors];  // All temperature sensors
float tempBatteryHigh;    // Highest battery temp
float tempBatteryLow;     // Lowest battery temp
float tempBatteryAverage; // Average battery temp
```

## State Data

The module populates a pack state structure:

```c
typedef struct {
    // Pack measurements
    float packVoltage;          // Total pack voltage
    float packCurrent;          // Current flowing out of pack
    float packPower;            // Calculated power
    
    // Cell data
    float cellVoltageHigh;
    float cellVoltageLow;
    float cellVoltageAverage;
    float cellVoltageMisMatch;
    driverLTC6803CellsTypedef cellVoltagesIndividual[NoOfCellsPossibleOnChip];
    
    // Status flags
    uint8_t disChargeLCAllowed;   // Discharge allowed (low current)
    uint8_t disChargeHCAllowed;   // Discharge allowed (high current)
    uint8_t chargeAllowed;        // Charge allowed
    
    // Error state
    modPowerElectronicsPackOperationalCellStatesTypedef packOperationalCellState;
    
    // Temperature
    float temperatures[NoOfTempSensors];
    float tempBatteryHigh;
    float tempBatteryLow;
} modPowerElectricsPackStateTypedef;
```

## Pre-charge Sequence

The pre-charge sequence protects loads from inrush current:

```
1. Initial State: All FETs OFF

2. Enable PRE-FET
   - Current limited through pre-charge resistor
   - Load capacitor begins charging

3. Monitor Load Voltage
   - Load voltage rises as capacitor charges

4. Validate
   - If load voltage > 75% of pack voltage:
     - Enable DIS-FET
     - Disable PRE-FET
     - Complete

5. Timeout Check
   - If timeout (5s default):
     - Transition to ERROR_PRECHARGE state
```

## Cell Balancing

Balancing equalizes cell voltages during charging:

**Algorithm:**
1. During charging, monitor cell voltages
2. When any cell exceeds threshold above average:
   - Enable balancing resistor on high cells
3. Continue until mismatch < threshold

**Configuration:**
- `maxMismatchThreshold` — Target mismatch (default 50mV)
- `balancingEnabled` — Enable/disable feature

## Safety Integration

The module integrates with the safety system:

- Monitors error conditions from cell monitoring
- Reports pack operational state (normal/error)
- Throttles current based on temperature/voltage
- Disables FETs on fault detection

## Hardware Details

### Pin Connections (STM32F3)

| Signal | GPIO | Function |
|--------|------|----------|
| CHG_FET | PB0 | Charge FET control |
| DIS_FET | PB1 | Discharge FET control |
| PRE_FET | PB2 | Pre-charge FET control |
| LTC_CS | PC4 | LTC6803 chip select |
| LTC_SCK | PC5 | LTC6803 SPI clock |
| LTC_MISO | PC6 | LTC6803 SPI MISO |
| LTC_MOSI | PC7 | LTC6803 SPI MOSI |

### Current Sense

- **Sense Resistor:** 0.5mΩ (200A range)
- **Amplifier:** ISL28022
- **I2C Address:** 0x44

## Related Documentation

- [Safety Checks](safety-checks.md) — Protection details
- [State Machine](state-machine.md) — FET control in state transitions
- [modPowerElectronics](../modules/modPowerElectronics.md) — Module reference