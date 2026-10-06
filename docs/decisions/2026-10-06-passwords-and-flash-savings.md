# A device password, and 34 KB of flash back

Date: 2026-10-06

## Flash: 496 KB to 462 KB with nothing removed

Measured with a linker map (`-Wl,-Map`, `--cref`), attributing each section to its
archive and object:

| Change | Saved |
|---|---|
| Drop `-u _printf_float -u _scanf_float` from the link (`scripts/link_options.py`) | 16.6 KB |
| lwIP without `LWIP_FEATURES` (`PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY_LOW_FLASH`) | 10.3 KB |
| No `Serial.begin()` (nothing was printed; drops HardwareSerial and the UART driver) | ~4 KB |
| `/status` time without `strftime()` | 3.3 KB |

- **Float printf/scanf.** The Arduino ESP8266 build forces newlib's float
  printf/scanf and, behind them, `dtoa`, `strtod` and `mprec`. Nothing here formats
  or parses a float with them: ArduinoJson and TFT_eSPI have their own float code.
  `tests/run_safety_tests.py` now fails on any `%f/%e/%g` format string or
  `scanf`/`toFloat` in `src/`, because such a format would silently print nothing
  on the device.
- **lwIP.** `LWIP_FEATURES` only adds IP forwarding/NAPT, IP fragmentation and
  reassembly, AutoIP, TCP SACK, three SNTP servers instead of one, and the hostname
  in DHCP DISCOVER. lwIP still sends the hostname in DHCP REQUEST, which is what
  routers record. HTTP here is TCP with a 536-byte MSS and never fragments.
- **Kept:** mDNS (19.9 KB, the largest optional piece). It is the easy way to find
  the clock on a home network. It is the next candidate if space runs out again.

With the features below, the image is 472,336 bytes (92% of 0x7E000), about 43 KB free.

## One password for the setup network and the settings page

Closes the [unauthenticated endpoints issue](../issues/2026-10-05-ap-and-ota-unauthenticated.md).

- **Generation.** On first boot: 10 characters from a 31-letter alphabet without
  0/o and 1/i/l (~49 bits), from the hardware RNG (`ESP.random()`). Long enough to
  resist an offline attack on a captured WPA2 handshake, where an 8-digit PIN would not.
- **Storage.** In the EEPROM sector at 0xFC0 (magic + up to 31 chars), not LittleFS,
  so recovery can read it even when storage does not mount. The whole sector is
  rewritten as with the power-cycle counter, so stock bytes survive.
- **Setup network:** WPA2 with the password.
- **Settings page:** sign in with the password to get a session cookie: a random
  128-bit token held in RAM (4 slots), `HttpOnly; SameSite=Strict`. A reboot signs
  everyone out. Five wrong attempts lock sign-in for 60 s, and each failure waits 0.8 s.
- **Guarded routes.** Every route that changes the clock checks the sign-in:
  settings, WiFi, scan, restart, firmware upload (refused at the first chunk, before
  anything is staged), photos, weather sync, backup/restore/reset, format and
  password change. `/config` and `/status` stay readable; neither holds a secret.
  The safety tests check each guard.
- **Finding the password.** It is on the clock's setup screen and recovery screen.
  The sign-in page has **Show the password on the clock**, which shows it on the
  panel for 60 s (at most once per 10 s), or until someone signs in, whichever
  comes first. Knowing it therefore needs sight of the clock.
- **Recovery.** Recovery is entered by power cycling or a startup crash. The setup
  network is open and nothing needs a sign-in there, so a forgotten password can
  never lock the owner out. Settings stay read-only in recovery, as before.

## Also in this change

- **Update check in the browser.** The settings page asks GitHub's API for the
  latest release (cached 6 h) and shows a banner if it is newer than the clock.
  The clock needs no internet access or HTTPS for this.
- **Night mode from sunset to sunrise.** `nightauto`: the Open-Meteo request also
  asks for today's sunrise and sunset.
- **Backup, restore and reset.** `GET /api/settings/export` returns the settings
  JSON, without WiFi details or the password. `POST /api/settings/import` applies
  it through the same validated `applySetting()` as `/api/set`. Reset deletes
  `/config.json` (photos and password stay). Forget WiFi clears the saved network.

## Shown so far

Against a local mock in the browser: sign-in (wrong and right password), the
update banner from the live GitHub release, the sunset schedule, backup/restore
(19 settings, and a non-backup file refused), password change, sign-out, and
phone width. Offline: the OTA test refuses an upload without sign-in before
staging. Not yet run on the device.
