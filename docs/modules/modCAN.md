# modCAN

CAN bus communication module.

## Overview

`modCAN` handles CAN communication with external devices.

## Key Functions

### Initialization

```c
void modCANInit(void);
```

### Main Task

```c
void modCANTask(void);
```

Called every 100ms. Handles TX/RX.

### Transmit

```c
void modCANSendStatus(modPowerElectricsPackStateTypedef* packState);
void modCANSendError(modPowerElectronicsPackOperationalCellStatesTypedef error);
void modCANSendCellInfo(void);
```

### Receive

```c
void modCANProcessRx(void);
// Process received commands (0x200)
```

## CAN Messages

| ID | Name | Direction | Description |
|----|------|-----------|-------------|
| 0x100 | BMS Status | TX | State, voltage, current, SOC |
| 0x101 | BMS Error | TX | Error flags |
| 0x102 | BMS Cell Info | TX | Cell voltages |
| 0x200 | BMS Command | RX | External commands |
| 0x201 | BMS Response | TX | Command responses |

## Configuration

```c
typedef struct {
    uint32_t canBitRate;    // 500000 default
    uint16_t canTxId;       // Base TX ID
    uint16_t canRxId;       // Base RX ID
} modConfigCANConfigTypedef;
```

## Hardware

- **Transceiver:** SN65HVD230
- **GPIO:** CAN_TX (PA12), CAN_RX (PA11)

## Related Documentation

- [CAN Communication](../systems/can-communication.md)