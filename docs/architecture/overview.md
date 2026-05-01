# Architecture Overview

The ENNOID BMS firmware is built on a modular architecture with clear separation of concerns. This document provides a high-level overview of the system.

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         Main Loop                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐  │
│  │  modOperational │  │ modPowerElectro-│  │   modCAN Task   │  │
│  │    State Task   │  │    nics Task    │  │                 │  │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘  │
│           │                    │                    │           │
│           └────────────────────┼────────────────────┘           │
│                                │                                  │
│  ┌─────────────────┐  ┌────────┴────────┐  ┌─────────────────┐  │
│  │   modHiAmp Task │  │ modDisplay Task │  │ modTerminal Task│  │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                                │
┌─────────────────────────────────────────────────────────────────┐
│                     Hardware Abstraction                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────────┐  │
│  │ STM32F3  │  │  LTC6803 │  │ ISL28022 │  │  Relay/HVBridge│  │
│  │   HAL    │  │  (cells) │  │ (current)│  │    (FETs)      │  │
│  └──────────┘  └──────────┘  └──────────┘  └────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Core Modules

### modOperationalState
**Purpose:** State machine management

Controls the operational state of the BMS. Manages transitions between:
- Initialization
- Charging
- Pre-charge
- Load enabled
- Balancing
- Power down
- Error states

### modPowerElectronics
**Purpose:** Power control and cell monitoring

- FET control (charge, discharge, pre-charge)
- Cell voltage monitoring via LTC6803
- Current sensing via ISL28022
- Temperature monitoring
- Cell balancing
- Safety fault detection

### modHiAmp
**Purpose:** High-current shield control

- Relay state machine
- Shunt-based current monitoring
- Temperature/humidity sensing (SHT21)
- I2C device management

### modCAN
**Purpose:** External communication

- BMS status broadcasting
- Command reception
- Safety status messages

### modConfig
**Purpose:** Configuration management

- Flash-based parameter storage
- Runtime configuration
- Default limits and thresholds

### modStateOfCharge
**Purpose:** SOC estimation

- Coulomb counting
- Voltage-based estimation
- Event-based updates

## Data Flow

1. **Input:** ADC readings (voltage, current, temperature)
2. **Processing:** Safety checks, state updates, SOC calculation
3. **Output:** FET control, CAN messages, display updates

## Task Scheduling

The firmware uses a super-loop architecture with time-triggered tasks:

| Task | Frequency | Purpose |
|------|-----------|---------|
| modOperationalStateTask | 10ms | State machine |
| modPowerElectronicsTask | 10ms | Power control |
| modHiAmpTask | 100ms | Shield monitoring |
| modCANTask | 100ms | CAN communication |
| modDisplayTask | 100ms | UI update |
| modTerminalTask | 100ms | CLI handling |

## Safety Architecture

Safety is implemented at multiple levels:

1. **Hardware Level:**
   - Hardware fuses
   - Hardware over-current protection
   - Redundant voltage sensing

2. **Software Level:**
   - Hard limits (immediate shutdown)
   - Soft limits (throttling/warning)
   - Timeout protection
   - State machine validation

See [Safety Checks](../systems/safety-checks.md) for details.

## Memory Layout

```
┌────────────────┐ 0x08000000
│   Bootloader   │  16KB
├────────────────┤
│                │
│   Application  │  ~200KB
│                │
├────────────────┤
│   Config Data  │  2KB (flash)
├────────────────┤
│   EEPROM Emul  │  2KB (flash)
└────────────────┘ 0x08040000
```

## Next Steps

- [Software Architecture](software.md) — Detailed module interactions
- [Hardware Architecture](hardware.md) — Circuit design
- [State Machine](../systems/state-machine.md) — Operational states
- [Safety Checks](../systems/safety-checks.md) — Protection details