# ENNOID-BMS-Firmware Documentation Plan

## 1. Proposed Directory Structure

```
ENNOID-BMS-Firmware/
├── docs/
│   ├── index.md                     # Main documentation index
│   ├── architecture/
│   │   ├── overview.md              # High-level system overview
│   │   ├── hardware.md              # Hardware architecture
│   │   └── software.md              # Software architecture
│   ├── systems/
│   │   ├── state-machine.md         # Operational state machine
│   │   ├── safety-checks.md         # Safety checks and protections
│   │   ├── can-communication.md     # CAN bus protocol
│   │   ├── configuration.md         # Configuration system
│   │   ├── soc-estimation.md        # State of Charge estimation
│   │   └── power-electronics.md     # Power electronics control
│   ├── modules/
│   │   ├── modOperationalState.md   # State machine module
│   │   ├── modPowerElectronics.md   # Power electronics module
│   │   ├── modHiAmp.md              # High amp/shield module
│   │   ├── modCAN.md                # CAN communication
│   │   ├── modConfig.md             # Configuration module
│   │   ├── modStateOfCharge.md      # SOC estimation
│   │   ├── modDisplay.md            # Display control
│   │   ├── modFlash.md              # Flash storage
│   │   ├── modTerminal.md           # CLI terminal
│   │   ├── modPowerState.md         # Power state detection
│   │   ├── modCommands.md           # Command handling
│   │   └── ...
│   ├── api/
│   │   └── (auto-generated from Doxygen)
│   ├── hardware/
│   │   ├── schematic.md             # Pinout and connections
│   │   └── bill-of-materials.md     # BOM
│   ├── getting-started/
│   │   ├── build.md                 # Build instructions
│   │   ├── flashing.md              # Flashing firmware
│   │   └── troubleshooting.md       # Common issues
│   └── development/
│       ├── coding-standards.md      # Code style guide
│       ├── contributing.md          # Contribution guidelines
│       └── debugging.md             # Debug tips
```

## 2. Documentation Format

**Recommendation: Markdown + Doxygen**

- **Markdown** (.md) — High-level documentation, guides, system explanations
- **Doxygen** — API documentation auto-generated from code comments
- **Mermaid** — State machine diagrams and flowcharts

## 3. High-Level System Documentation

### A. State Machine (modOperationalState)

**States:**

| State | Description |
|-------|-------------|
| `OP_STATE_INIT` | Initialization, waits for button press or charger |
| `OP_STATE_CHARGING` | Charging mode, manages charge current |
| `OP_STATE_PRE_CHARGE` | Pre-charge sequence to protect load |
| `OP_STATE_LOAD_ENABLED` | Normal operation, battery powering load |
| `OP_STATE_FORCEON` | Forced on state (bypass protection) |
| `OP_STATE_BALANCING` | Cell balancing mode |
| `OP_STATE_POWER_DOWN` | Safe shutdown sequence |
| `OP_STATE_BATTERY_DEAD` | Battery too low to operate |
| `OP_STATE_ERROR_PRECHARGE` | Pre-charge failure |

**State Transitions:**

```
INIT → CHARGING        (charger detected)
INIT → PRE_CHARGE      (button pressed)
PRE_CHARGE → LOAD_ENABLED  (pre-charge complete)
LOAD_ENABLED → POWER_DOWN (no current for timeout)
Any state → ERROR_*    (safety fault detected)
```

### B. Safety Checks (modPowerElectronics)

**Voltage Protection:**
- Hard cell voltage limits — immediate shutdown
- Soft cell voltage limits — throttle or warn
- Pack voltage monitoring
- Pre-charge voltage validation

**Current Protection:**
- Over-current detection (pack current)
- High-current load protection (modHiAmp)
- Short circuit detection

**Temperature Protection:**
- Battery temperature limits
- BMS internal temperature
- External temperature sensors (NTC)
- Fan speed control based on temp

**Error States:**

```c
typedef enum {
    PACK_STATE_ERROR_HARD_CELLVOLTAGE,  // Hard voltage fault
    PACK_STATE_ERROR_SOFT_CELLVOLTAGE,  // Soft voltage fault
    PACK_STATE_ERROR_OVER_CURRENT,      // Over-current fault
    PACK_STATE_NORMAL                   // Normal operation
} modPowerElectronicsPackOperationalCellStatesTypedef;
```

### C. Other Key Systems

#### Power Electronics (modPowerElectronics)
- FET control (charge/discharge/pre-charge)
- Cell voltage monitoring (LTC6803)
- Current sensing (ISL28022)
- Temperature sensing
- Cell balancing

#### High Amp Module (modHiAmp)
- Relay controller state machine
- Shunt-based current monitoring
- Temperature/humidity sensing (SHT21)
- I2C device management

#### CAN Communication (modCAN)
- BMS-to-external device protocol
- Safety status broadcasting
- Command reception

#### Configuration (modConfig)
- Flash-based parameter storage
- Runtime configuration management
- Default values and limits

#### State of Charge (modStateOfCharge)
- Coulomb counting
- Voltage-based estimation
- Event-based updates (full/empty)

## 4. Module Summary

| Module | Purpose |
|--------|---------|
| modOperationalState | State machine management |
| modPowerElectronics | Power FETs, cell monitoring, safety |
| modHiAmp | High-current shield, relays, sensors |
| modCAN | CAN bus communication |
| modConfig | Configuration and flash storage |
| modStateOfCharge | SOC estimation |
| modDisplay | Display UI control |
| modFlash | Non-volatile storage |
| modTerminal | CLI interface |
| modPowerState | Power/button state detection |
| modCommands | Command parsing |
| modUART | Serial communication |
| modEffect | LED effects |
| modDelay | Timing utilities |
| modMessage | Message handling |

---

Want me to create any of these documentation files?