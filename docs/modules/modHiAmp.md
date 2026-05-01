# modHiAmp

High-current shield module - relay controller, shunt monitoring, external sensors.

## Overview

`modHiAmp` controls the high-current shield, which provides:
- Relay control for main current path
- Shunt-based current monitoring
- Temperature/humidity sensing (SHT21)
- I2C device management

## Key Functions

### Initialization

```c
void modHiAmpInit(void);
```

### Main Task

```c
void modHiAmpTask(void);
```

Called every 100ms. Handles relay state machine and monitoring.

### Current Monitoring

```c
float modHiAmpGetCurrent(void);
// Returns current in Amps (shunt-based)
```

### Relay Control

```c
void modHiAmpSetRelay(bool state);
// Control main relay
```

## Hardware

- **MCU:** ATtiny (separate processor)
- **Communication:** I2C
- **Sensors:** SHT21 (temperature/humidity)
- **Current:** Shunt resistor + op-amp

## Relay State Machine

```
[OFF] → [PRECHARGE] → [ON]
  ↑______________________↓
    (fault or disable)
```

States:
1. **OFF** — Relay open
2. **PRECHARGE** — Pre-charge cycle
3. **ON** — Relay closed

## Safety Features

- Over-current detection
- Temperature monitoring
- Humidity monitoring
- Communication watchdog

## Related Documentation

- [Power Electronics](../systems/power-electronics.md)
- [Safety Checks](../systems/safety-checks.md)