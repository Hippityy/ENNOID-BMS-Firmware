# ENNOID-BMS-Firmware Documentation

Welcome to the ENNOID BMS Firmware documentation. This documentation covers the battery management system firmware for ENNOID hardware.

## Quick Links

- [Architecture Overview](architecture/overview.md)
- [State Machine](systems/state-machine.md)
- [Safety Checks](systems/safety-checks.md)
- [CAN Communication](systems/can-communication.md)
- [Configuration](systems/configuration.md)
- [Getting Started](getting-started/build.md)

## What is ENNOID BMS?

ENNOID BMS is a Battery Management System (BMS) firmware designed for lithium-ion battery packs. It provides:

- **Cell Monitoring** — Voltage and temperature monitoring for up to 12 cells
- **Safety Protection** — Over-voltage, under-voltage, over-current, and temperature protection
- **State Machine** — Defined operational states (Init, Charging, Pre-charge, Load Enabled, etc.)
- **CAN Communication** — External device communication via CAN bus
- **Configuration** — Flash-based configurable parameters
- **SOC Estimation** — State of Charge estimation

## Key Components

| Component | Description |
|-----------|-------------|
| [State Machine](systems/state-machine.md) | Operational state management |
| [Safety Checks](systems/safety-checks.md) | Voltage, current, and temperature protection |
| [Power Electronics](systems/power-electronics.md) | FET control and cell monitoring |
| [CAN Bus](systems/can-communication.md) | External communication protocol |
| [Configuration](systems/configuration.md) | System parameters and limits |

## Hardware

- **MCU:** STM32F3xx (STM32F302/STM32F303)
- **Cell Monitor:** LTC6803-2 (12-cell IC)
- **Current Sense:** ISL28022
- **High-Current Shield:** Custom relay controller with shunt monitoring

## Getting Started

New to the firmware? Start here:

1. [Build the Firmware](getting-started/build.md)
2. [Flash the Device](getting-started/flashing.md)
3. [Troubleshooting](getting-started/troubleshooting.md)

## Module Reference

Individual module documentation:

- [modOperationalState](modules/modOperationalState.md) — State machine
- [modPowerElectronics](modules/modPowerElectronics.md) — Power control
- [modHiAmp](modules/modHiAmp.md) — High-current shield
- [modCAN](modules/modCAN.md) — CAN communication
- [modConfig](modules/modConfig.md) — Configuration
- [modStateOfCharge](modules/modStateOfCharge.md) — SOC estimation

## Support

For issues and questions, refer to the [Troubleshooting](getting-started/troubleshooting.md) guide or open an issue on GitHub.