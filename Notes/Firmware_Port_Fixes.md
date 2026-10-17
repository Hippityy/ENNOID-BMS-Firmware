# HV-2 port: first corrective pass

This is a development reconstruction, identified as `V6.6-port.1` with test version1.
It is not a verified reproduction of vendor6.06. The6.06 tool archive bundles an
HV-2 image reporting6.05. No device was flashed during this work.

## Implemented

- BMS command IDs240-247 and the tool's exact96-field/268-byte configuration wire
  schema. Decode into a temporary object, check lengths and values, then publish.
- Shared cell/channel map for acquisition and balancing, including short first/last
  ICs and parallel strings. Validate monitor capacity, array bounds and population.
- Correct charge/discharge overcurrent polarity, a separate charge-current limit,
  latched inhibition, temperature indexing and sequencing errors in array flattening.
- CAN queued nonblocking transmission with128 normal slots and a reserved heartbeat
  slot. Active mailbox deadline50ms, abort completion deadline50ms, queued-frame
  maximum age250ms. Drop stale queued work on bus-off; ABOM remains enabled.
- Independent FIFO receive handling, bounded IRQ/main-loop drain, valid DLC/RTR
  checks and fragmentation bounds. No dependency on legacy HAL shared RX/TX state.
- Remove the changing-ID receive watchdog. Failed initialization returns, with a
  bounded250ms retry interval; it cannot hang the BMS main loop. Recovery preserves
  transmit schedules and safety deadlines. SysTick has priority over CAN RX.
- VESC packet45 heartbeat at10Hz, slow telemetry1Hz, correct Ah/Wh values and balance
  low byte. Existing56-bit balance-mask protocol cannot describe cells beyond55.
- Safety/charger heartbeat freshness uses IRQ arrival time, including after queuing,
  rather than time of processing. Reinitialization never counts as a safety heartbeat.
- `can_status` exposes counters and ESR; `storage_status` exposes persistence state.
- Persistence has a distinct versioned bank, CRC32, commit markers and board/schema
  checks. Preserve the old bank; interrupted or invalid writes fail closed.
- Blank, unknown or unversioned settings boot into UART commissioning with power
  paths disabled and the open-drain safety output released. Runtime entry also sends
  zero balancing masks using the initialized monitor driver. SPI failure can still
  prevent a physical monitor from accepting that command; confirm on hardware.
- Tool configuration edits are staged; GET returns the pending values while
  sensing continues with the old validated configuration. Saving latches output
  shutdown until reboot. An existing trusted CAN connection can finish store ACKs
  and diagnostics with its old settings. Force-on no longer erases calibration.

## Commissioning and migration

Export the installed board's settings before an upgrade. Unknown vendor EEPROM
layouts are deliberately not guessed. Boot recovery communicates over UART; CAN
and power-electronics operation do not start with untrusted configuration.

Use the tool to read defaults, explicitly set all real pack/calibration/protection
values, write/store configuration, then reboot. A write acknowledgement means the
268-byte payload passed validation and is staged; a store acknowledgement means persistence
succeeded. New values become operational only after reboot. Recovery remains
latched until reboot even after a successful save.

For an EEPROM known from provenance to be the open-source312-byte baseline or
316-byte previous port, the UART terminal commands `storage_migrate312` or
`storage_migrate316` perform a validated explicit conversion, preserving SOC and
the original bank. Then reboot. **Do not choose a layout from byte count alone:** a
torn316-byte write can resemble a312-byte layout. These commands reject malformed
data but cannot prove the provenance of semantically plausible bytes.

Currently supported sensing/algorithm choices are ISL28022 current, ISL28022 or
summed-cell voltage, coulomb SOC and chemistry0. Unsupported dual/CAN/INA sensing,
ELCON charger, other chemistry/SOC selections and unfinished override/calibration
commands are rejected rather than silently claiming functionality. Delta-Q uses
the existing CANopen implementation. These restrictions need further work if your
actual pack relies on another sensor or charger.

## Verification

Run `bash tests/run_all.sh /absolute/path/to/bms-tool/res/config.xml` for host
ASAN/UBSAN checks of production modules: exhaustive cell maps, physical balancing,
current/temperature protection, independent XML wire fixture, command bounds,
storage migrations and every write interruption, queue ordering/backpressure,
ACK loss/abort/bus-off, stale heartbeats, tick wrap and actual CAN register glue.

Build HV-2 with `make -B -j4 build_args=-DENNOID_HV_2=1`. Header dependencies are
generated; force a rebuild when changing board compiler flags. No flashing command
is part of validation. Legacy compiler warnings remain outside the corrected paths.

Fake MMIO checks write encodings, not silicon FIFO/W1C/arbitration behavior. Flash
page-transfer interruption, contactor/safety polarity, voltage/current calibration,
monitor PEC behavior, fan disturbance and complete CAN loss require an instrumented
HV-2 bench test. The long independent watchdog remains unchanged until startup and
blocking diagnostic timing are characterized. No guaranteed recovery latency or
accumulator protection certification is implied by host tests or compilation.
