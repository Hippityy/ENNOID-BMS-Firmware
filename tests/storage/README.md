# Persistent configuration checks

Run `./tests/storage/run.sh` with a C99 compiler. The script compiles the actual
storage manager, config migration/validation code, and cell mapper with address
and undefined-behavior sanitizers. Only the HAL EEPROM boundary is mocked.

Coverage includes:

- Explicit migration of the known open-source 312-byte and backport 316-byte
  config ABIs, with bit-for-bit preservation of the legacy bank and all four SOC
  floats, followed by loading the new versioned image on reboot.
- Blank, gapped, unknown-address, invalid-header, malformed-config, and NaN-SOC
  images; failed flash unlock/init; unknown schema and hardware tag; CRC failure.
- Refusal to infer an old ABI automatically: a torn 316-byte config plus three
  SOC floats has the same extent as a complete 312-byte config plus four floats.
  The operator must know which open-source firmware wrote the original image.
  Stock/vendor layouts are unsupported; these commands cannot establish their
  provenance or reconstruct a partial image.
- Failure at every halfword write in migration, normal config save, and SOC save.
  Interrupted updates retain the original unversioned bank and fail closed if
  a versioned transaction is incomplete. A failed invalidation that writes
  nothing can still leave the previously committed image usable.
- Explicit commissioning of blank storage, zero initial SOC, and subsequent SOC
  persistence. No pack configuration defaults are automatically made operational.

The new virtual address bank uses schema/length/hardware metadata, separate
config and SOC CRC32 checksums, and independently invalidated/committed markers.
The address-transfer table retains both legacy and new banks and is initialized
before the HAL EEPROM recovery routine. Its maximum 376 live keys fit within
one 2-KiB emulation page (511 record capacity).

The mock compacts each virtual-word image; it does **not** simulate STM32 flash
programming physics, HAL page-transfer power loss, or real target reset behavior.
Target tests must exercise those cases before deployment. CRC protects accidental
corruption, not authenticity. As with any legacy format without a CRC/commit
marker, an operator-selected old ABI cannot distinguish every plausible torn
or corrupted record from a complete one.
