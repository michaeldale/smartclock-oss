# Recovery without a button

Date: 2026-10-05

## Decision

GPIO4 is never read to choose recovery. Recovery is entered by three boots
that each end within 10 s (quick power cycles, or a crash loop), by a crash
in the first 30 s of normal operation, or by a filesystem that will not mount.

## Why

The unit has no visible button. Stock configures GPIO4 as `INPUT_PULLUP` with
a FALLING interrupt, so something may be wired there, but its idle level is
unknown. If it idled low, a button check would have put every boot into
recovery.

A setup AP needs no button: it is always on, alongside home WiFi.

## How it works

- **Power cycles:** a 5-byte record (magic + count) at offset 0xFF0 of the
  EEPROM sector (0x3FB000). RTC memory does not survive power loss, so it
  cannot hold this count. `EEPROM.begin(4096)` loads the whole sector so stock
  bytes survive the rewrite, and `EEPROM.end()` erases only if a byte changed.
  A boot that lasts 10 s clears the count.
- **Crashes:** an RTC marker set before storage and display start, cleared
  after 30 s. A second state marks the recovery panel start, so a panel that
  resets the board is skipped on the next boot.
- **Deliberate reboots** (`/restart`, `/connect`, OTA, format) go through
  `safeRestart()`, which clears both, so a quick save is not taken for a crash.

## Result

The power-cycle sequence was tried on the device after the first flash and
entered recovery as intended (reported 2026-10-05). The counter logic is also
covered offline by `tests/ota_harness.cpp`.
