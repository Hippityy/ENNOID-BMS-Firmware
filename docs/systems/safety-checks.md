# Safety Checks

The BMS implements multiple layers of safety protection to prevent damage to the battery pack, connected devices, and the BMS itself.

## Safety Architecture

Safety protection is implemented at multiple levels:

| Level | Type | Response Time |
|-------|------|---------------|
| Hardware | Fuses, TVS, hardware comparators | Instant |
| Software - Hard | Immediate shutdown | <10ms |
| Software - Soft | Throttling, warnings | 100ms |
| Software - Monitoring | State machine validation | Per cycle |

## Error States

```c
typedef enum {
    PACK_STATE_ERROR_HARD_CELLVOLTAGE,  // Hard voltage fault
    PACK_STATE_ERROR_SOFT_CELLVOLTAGE,  // Soft voltage fault
    PACK_STATE_ERROR_OVER_CURRENT,      // Over-current fault
    PACK_STATE_NORMAL                   // Normal operation
} modPowerElectronicsPackOperationalCellStatesTypedef;
```

## Voltage Protection

### Cell Voltage Monitoring

The BMS monitors each cell voltage using the LTC6803-2 IC.

**Hard Limits (Immediate Shutdown):**
- Over-voltage (OV): Cell > 4.2V (configurable)
- Under-voltage (UV): Cell < 2.5V (configurable)

**Soft Limits (Throttling/Warning):**
- Soft OV: Cell > 4.15V (reduce charge current)
- Soft UV: Cell < 2.8V (reduce discharge current)

### Pack Voltage

- Total pack voltage monitoring
- Pre-charge validation (load voltage must reach 75% of pack voltage)

### Voltage Throttling

The system implements current throttling based on cell voltages:

```
Cell Voltage    | Charge Limit | Discharge Limit
---------------|--------------|----------------
> 4.15V        | Throttle     | Full
2.8V - 4.15V   | Full         | Full
< 2.8V         | Full         | Throttle
< 2.5V         | Full         | Shutdown
```

## Current Protection

### Over-Current Detection

**Pack Current:**
- Hardware-level current sense amplifier
- ADC reading converted to current
- Configurable threshold (default: 200A discharge, 100A charge)

**High-Current Load (modHiAmp):**
- Shunt-based monitoring
- Separate relay controller
- Pre-charge current limiting

### Short Circuit Protection

- Fast-acting hardware protection (FETs)
- Software monitors for sudden current spikes
- Automatic shutdown on detection

### Current Limits

| Condition | Limit | Action |
|-----------|-------|--------|
| Discharge (hard) | Configurable | Immediate shutdown |
| Discharge (soft) | 80% of hard | Throttle |
| Charge (hard) | Configurable | Immediate shutdown |
| Charge (soft) | 80% of hard | Throttle |
| Pre-charge | Limited | Controlled ramp |

## Temperature Protection

### Temperature Sensors

The BMS monitors multiple temperature points:

**Internal (BMS):**
- STM32 internal temperature
- LTC6803 chip temperature

**External (Battery/Pack):**
- Up to 13 NTC thermistors
- Configurable placement (cells, connectors, PCB)

**External (Shield - modHiAmp):**
- Temperature sensing (SHT21)
- Humidity sensing

### Temperature Limits

| Sensor | Warning | Critical | Action |
|--------|---------|----------|--------|
| Battery NTC | 45°C | 55°C | Throttle at warning, shutdown at critical |
| BMS IC | 70°C | 85°C | Throttle, then shutdown |
| External NTC | 60°C | 70°C | Throttle, then shutdown |

### Fan Control

The BMS can control a fan (EMC2305 driver) based on temperature:
- Configurable start temperature
- Configurable fan curve
- RPM monitoring

## Pre-Charge Safety

Pre-charge is a critical safety sequence to protect loads from inrush current.

### Pre-Charge Sequence

1. **Check conditions** — Battery voltage OK, no faults
2. **Enable pre-charge FET** — Limited current path
3. **Monitor voltage** — Load voltage must rise
4. **Validate** — Load voltage > 75% of pack voltage
5. **Enable discharge FET** — Full current path
6. **Disable pre-charge** — Transition complete

### Pre-Charge Failures

- **Timeout** — Didn't complete in time (default 5s)
- **Voltage not rising** — Open circuit or bad load
- **Pack voltage too low** — Insufficient energy

## CAN Safety Messages

The BMS broadcasts safety status via CAN:

| Message | ID | Content |
|---------|-----|---------|
| BMS Status | 0x100 | State, voltage, current, temp |
| BMS Error | 0x101 | Error flags, fault details |
| BMS Cell Info | 0x102 | Cell voltages, balancing status |

### Safety Flags

```c
// Safety status flags (broadcast via CAN)
uint8_t safetyOverCANHCSafeNSafe;  // High-current safety
uint8_t packInSOA;                  // Safe Operating Area
uint8_t watchDogTime;              // Watchdog status
```

## Watchdog

The system includes a watchdog timer:
- Independent hardware watchdog (IWDG)
- Must be reset periodically by main task
- Timeout triggers system reset

## Configuration

Safety limits are configurable via `modConfig`:

```c
typedef struct {
    // Voltage limits
    float maxCellVoltageHard;      // Hard OV threshold
    float maxCellVoltageSoft;      // Soft OV threshold
    float minCellVoltageHard;      // Hard UV threshold
    float minCellVoltageSoft;      // Soft UV threshold
    
    // Current limits
    float maxDischargeCurrentHard;
    float maxDischargeCurrentSoft;
    float maxChargeCurrentHard;
    float maxChargeCurrentSoft;
    
    // Temperature limits
    float maxBatteryTempHard;
    float maxBatteryTempSoft;
    
    // Timing
    uint32_t timeoutLCPreCharge;
    uint32_t notUsedTimeout;
    
    // Other
    float maxMismatchThreshold;    // Cell imbalance threshold
} modConfigGeneralConfigStructTypedef;
```

## Best Practices

1. **Always use hard limits** — Soft limits are advisory, hard limits are mandatory
2. **Test fault responses** — Verify shutdown behavior under fault conditions
3. **Configure appropriately** — Match limits to battery chemistry and application
4. **Monitor logs** — Review fault logs to identify recurring issues
5. **Redundancy** — Don't rely solely on software; hardware protection is essential

## Related Documentation

- [State Machine](state-machine.md) — How safety affects state transitions
- [Power Electronics](power-electronics.md) — FET control and hardware
- [modHiAmp](../modules/modHiAmp.md) — High-current shield safety
- [Configuration](configuration.md) — How to set safety parameters