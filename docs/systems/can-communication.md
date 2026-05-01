# CAN Communication

The BMS uses CAN (Controller Area Network) bus for communication with external devices.

## Overview

- **Protocol:** CAN 2.0B (extended identifiers)
- **Bit Rate:** 500 kbps (configurable)
- **Termination:** 120Ω required at both ends

## Message Overview

| Message | CAN ID | Direction | Frequency |
|---------|--------|-----------|-----------|
| BMS Status | 0x100 | TX | 100ms |
| BMS Error | 0x101 | TX | On event |
| BMS Cell Info | 0x102 | TX | 1s |
| BMS Command | 0x200 | RX | As needed |
| BMS Response | 0x201 | TX | On event |

## Message Definitions

### BMS Status (0x100)

Broadcasts general BMS status.

**Data Layout (8 bytes):**

| Byte | Field | Type | Description |
|------|-------|------|-------------|
| 0 | State | uint8 | Operational state (0-8) |
| 1 | Flags | uint8 | Status flags |
| 2-3 | Pack Voltage | uint16 | Pack voltage (0.1V) |
| 4-5 | Pack Current | int16 | Pack current (0.1A) |
| 6-7 | SOC | uint16 | State of Charge (0.1%) |

**Flags:**
- Bit 0: Charging allowed
- Bit 1: Discharging allowed
- Bit 2: Balancing active
- Bit 3: Charger connected

### BMS Error (0x101)

Broadcasts error conditions.

**Data Layout (8 bytes):**

| Byte | Field | Type | Description |
|------|-------|------|-------------|
| 0 | Error Code | uint8 | Error type |
| 1 | Error Subcode | uint8 | Detailed error |
| 2 | Cell Index | uint8 | Affected cell (if applicable) |
| 3-7 | Reserved | - | - |

**Error Codes:**
- 0x01: Over-voltage
- 0x02: Under-voltage
- 0x03: Over-current
- 0x04: Overtemperature
- 0x05: Pre-charge failure
- 0x06: Communication error

### BMS Cell Info (0x102)

Broadcasts detailed cell information.

**Data Layout (8 bytes):**

| Byte | Field | Type | Description |
|------|-------|------|-------------|
| 0-1 | Cell High | uint16 | Highest cell voltage (mV) |
| 2-3 | Cell Low | uint16 | Lowest cell voltage (mV) |
| 4-5 | Cell Avg | uint16 | Average cell voltage (mV) |
| 6-7 | Mismatch | uint16 | Voltage mismatch (mV) |

### BMS Command (0x200)

Receives commands from external device.

**Supported Commands:**

| Byte 0 | Command | Parameters |
|--------|---------|------------|
| 0x01 | Reset | - |
| 0x02 | Set State | Byte 1: Target state |
| 0x03 | Get Config | - |
| 0x04 | Set Config | Bytes 1-4: Config value |
| 0x05 | Read Cell | Byte 1: Cell index |
| 0x10 | Force On | Byte 1: Enable (1) / Disable (0) |

### BMS Response (0x201)

Response to commands.

**Data Layout:**

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Command | Echo of received command |
| 1 | Status | 0x00: Success, 0x01: Error |
| 2-7 | Data | Response data |

## Configuration

CAN parameters configurable via `modConfig`:

```c
typedef struct {
    uint32_t canBitRate;        // CAN bit rate (default: 500000)
    uint16_t canTxId;           // Transmit ID base
    uint16_t canRxId;           // Receive ID base
} modConfigCANConfigTypedef;
```

## Hardware

- **Transceiver:** SN65HVD230 or equivalent
- **GPIO:** CAN_TX (PA12), CAN_RX (PA11) on STM32F3
- **Termination:** 120Ω at both ends of bus

## Error Handling

CAN errors are monitored and reported:
- Bus-off condition
- TX/RX timeout
- Invalid message format

## Best Practices

1. **Terminate properly** — 120Ω at both ends
2. **Check voltage** — CANH/CANL should be ~2.5V at idle
3. **Use shielded cable** — For noisy environments
4. **Monitor errors** — Check BMS Error messages for issues

## Related Documentation

- [State Machine](state-machine.md) — Operational states
- [Safety Checks](safety-checks.md) — Error handling
- [modCAN](../modules/modCAN.md) — Module reference