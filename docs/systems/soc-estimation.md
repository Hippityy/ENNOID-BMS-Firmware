# State of Charge (SOC) Estimation

The BMS estimates the State of Charge (SOC) of the battery pack using multiple methods.

## Overview

SOC represents the remaining capacity as a percentage:
- 100% = Full
- 0% = Empty

The BMS uses a combination of:
1. **Coulomb Counting** — Integrated current over time
2. **Voltage-based Estimation** — Open-circuit voltage lookup
3. **Event-based Updates** — Full/empty detection

## Methods

### 1. Coulomb Counting

The primary method tracks charge entering and leaving the battery:

```
SOC = SOC_initial - (∫I dt / Capacity)
```

**Algorithm:**
1. Start with known SOC (e.g., 100% after full charge)
2. Continuously integrate current: `capacity += I * dt`
3. Calculate SOC: `SOC = 100 - (capacity_discharged / total_capacity)`

**Pros:**
- Accurate during steady operation
- Works in real-time

**Cons:**
- Drift over time
- Requires initial accurate SOC
- Current sensor offset affects accuracy

### 2. Voltage-based Estimation

When coulomb counting may have drifted, voltage-based estimation provides a reference:

**Lookup Table (Li-ion typical):**

| Voltage (per cell) | SOC |
|--------------------|-----|
| 4.20V | 100% |
| 4.00V | 80% |
| 3.70V | 50% |
| 3.50V | 30% |
| 3.20V | 10% |
| 3.00V | 0% |

**Usage:**
- Used to correct coulomb counting drift
- Only valid when battery is at rest (no current)
- Applied periodically or when current is low

### 3. Event-based Updates

Certain events reset or correct the SOC estimation:

| Event | Action |
|-------|--------|
| Full charge detected | Set SOC = 100% |
| Deep discharge | Set SOC = 0% |
| Long rest period | Apply voltage-based correction |

## Implementation

### Data Structure

```c
typedef struct {
    float generalStateOfCharge;      // Current SOC (0-100%)
    float generalStateOfChargeAverage; // Smoothed SOC
    float socCapacityAh;             // Available capacity (Ah)
    float socFullCapacityAh;         // Full capacity (Ah)
    float socRemainingCapacityAh;    // Remaining capacity (Ah)
    float socCycleCount;             // Charge cycles
    uint8_t socFullyCharged;         // Full flag
    uint8_t socFullyDischarged;      // Empty flag
} modStateOfChargeStructTypeDef;
```

### Functions

```c
// Initialize
void modStateOfChargeInit(modStateOfChargeStructTypeDef* socHandle, 
                          modConfigGeneralConfigStructTypedef* config);

// Update SOC based on current integration
void modStateOfChargeUpdateCoulomb(uint32_t tick, float current);

// Apply voltage-based correction
void modStateOfChargeUpdateVoltage(float voltage);

// Handle events (full/empty)
void modStateOfChargeVoltageEvent(stateOfChargeEventTypedef event);

// Get current SOC
float modStateOfChargeGetGeneral(void);
```

## Configuration

SOC estimation parameters in `modConfig`:

```c
typedef struct {
    float ratedCapacityAh;           // Battery capacity (Ah)
    float socInitial;                // Initial SOC on first boot (%)
    float socVoltageCorrectionThreshold; // Current threshold for voltage correction (A)
    uint32_t socRestTime;            // Rest time required for voltage correction (s)
} modConfigSOCConfigTypedef;
```

## Accuracy Considerations

### Factors Affecting Accuracy

1. **Current Sensor Accuracy** — Offset and gain errors accumulate
2. **Temperature** — Affects capacity
3. **Aging** — Battery capacity degrades over time
4. **Self-discharge** — Not accounted for in coulomb counting

### Improving Accuracy

1. **Regular Full Charges** — Reset SOC to 100%
2. **Voltage Corrections** — Apply when battery at rest
3. **Temperature Compensation** — Adjust for temperature
4. **Capacity Tracking** — Update capacity based on cycling

## Display

SOC is displayed on the LCD:

- Percentage (0-100%)
- Visual bar indicator
- Estimated runtime (when discharging)

## Related Documentation

- [State Machine](state-machine.md) — How SOC affects states
- [Safety Checks](safety-checks.md) — Low voltage protection
- [modStateOfCharge](../modules/modStateOfCharge.md) — Module reference