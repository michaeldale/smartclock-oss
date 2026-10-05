# Firmware migration and recovery

Installed over stock SD Pro V1.0.6 from its own update page on 2026-10-05, and updated
through our own updater several times since
([record](decisions/2026-10-05-first-flash-over-stock.md)). One unit has been tested.
Going back to stock still needs UART, so a full flash backup over UART beforehand is the
only way to keep the vendor firmware; the vendor update binaries are not full-device
backups. Release images come from `python scripts/make_release.py` and are already named
`SDPro_SmartClockOSS_v<version>.bin`.

## Build and first flash

1. Build with `pio run`. The image is `.pio/build/esp12e/firmware.bin`.
2. Run `python tests/run_safety_tests.py` against that build (see compiler requirements
   in the script). These are offline tests, not hardware validation.
3. Verify that the device is ESP8266 with 4 MB flash and supports the raw sketch image.
   The build uses DIO at 40 MHz and the **4m3m** layout, the same one the stock firmware
   uses. Both stock images mount LittleFS at `0x40300000`–`0x405FA000` (flash offsets
   0x100000–0x3FA000) with 256 B pages and 8 KB blocks, and our image carries identical
   constants (checked by the safety tests). So after OTA our firmware mounts the existing
   stock volume. Our `/config.json` does not collide with stock's `/theme_config.txt` or
   `/photo_config.json`.
   With 4m3m the first 1 MB holds the running sketch **and** the staged update, so an
   image must stay under 0x7E000 bytes (about 504 KB). `scripts/build_options.py` fails the
   build and deletes `firmware.bin` above that. The first flash through the stock updater
   allows about 552 KB (1 MB minus the 488 KB stock image).
4. Establish recovery and back up the device before using its stock OTA page. The stock
   updater controls the first transfer; our validation applies only to subsequent uploads.
5. If proceeding with the stock updater, use the built firmware image and stable power.
   **Rename it to start with `SDP`** (for example `SDPro_SmartClockOSS.bin`): the stock
   page refuses any other filename in the browser, with "Please use the correct
   firmware". It does not check the contents
   ([record](decisions/2026-10-05-first-flash-over-stock.md)). Our own update page
   ignores the filename.
   Do not interrupt power during upload or reboot/copying.

## Recovery access

The firmware starts an open AP, **Smart Weather Clock**, at **http://192.168.4.1/** and
starts its embedded web server before any storage or display initialization. The AP stays
up in every mode, alongside home WiFi. No button is needed to reach it. While the saved
network is missing, the STA side searches in 30 s windows every 5 minutes, because
continuous searching makes the ESP8266 hop channels and drop AP clients. On the LAN the
clock also answers at **http://smartclock-XXXXXX.local/** (mDNS, from the same
subnet only), where XXXXXX is from the chip id; the same name is its DHCP
hostname, so the router's client list shows it.

The unit has no usable button, so GPIO4 is never read to choose recovery. Recovery starts
when any of these happen:

- **Three quick power cycles** (power-on resets only; a crash or restart clears the
  count). Power the clock on for less than 10 s, twice, then
  power it on and leave it. The count is stored in a 5-byte record at offset 0xFF0 of the
  EEPROM sector (RTC memory does not survive power loss). The whole sector is read and
  rewritten, so stock EEPROM bytes are preserved. A boot that lasts 10 s clears the count.
  A crash loop shorter than 10 s triggers recovery the same way.
- **A crash during startup.** An RTC marker makes the next reset enter recovery if
  normal operation has not survived its first 30 seconds. Every deliberate reboot
  (`/restart`, `/connect`, OTA, format) goes through `safeRestart()`, which clears both
  triggers so a fast save is never mistaken for a crash.
- **A filesystem mount failure.** The firmware sets `LittleFSConfig(false)`, so it does
  **not** format an unknown or corrupt volume. Recovery then offers an explicit **Format
  storage** button (POST `/format_storage?confirm=erase`). It is accepted only after a
  mount actually failed and erases the whole FS region.

In recovery the screen shows "Recovery", the AP name and address, and the LAN IP if one is
connected. If the panel itself reset the previous recovery boot, the panel is skipped. In
recovery, settings requests return HTTP 503. Only OTA, format (after a failed mount),
status, and restart work. **Restart** leaves recovery.

### Build-time WiFi defaults (optional)

Copy `secrets.example.ini` to `secrets.ini` (gitignored) and set `[wifi] ssid/password`.
These are compiled in and used only when no WiFi is saved, and also in recovery. So even
with broken storage the clock joins the LAN and can be updated there. The values are
stored in plain text inside `firmware.bin`, so do not share that build. The WiFi page on
the clock (first card at http://192.168.4.1/) remains the normal way to set WiFi.

### Returning to stock

Stock images lack the length/CRC fields that our updater requires, so **you cannot OTA
back to the vendor firmware**. Returning to stock needs UART. Back up the full flash over
UART before the first flash if you may ever want to go back.

The AP and OTA endpoint still have no authentication. Anyone within WiFi range can
reflash the device. Firmware recovery cannot rescue an image that never reaches this code;
UART remains the fallback.

## Subsequent updates

Use the embedded **Firmware Update (OTA)** form, or POST a single multipart firmware
file to `/update_ota`. Only raw project-compatible ESP8266 Arduino images are accepted:
4 MB/DIO/40 MHz, with the embedded image length and CRC produced by `elf2bin.py`.
Compressed, filesystem, and vendor binaries without these fields are unsupported.
The filename never selects a flash region; uploads always use `U_FLASH`.

The updater buffers and validates the image header before staging. It checks the declared
length against available sketch space, accounts for every received byte, and verifies the
CRC of the entire image with the embedded length/CRC fields zeroed. It checks each updater
operation and withholds the final byte until the complete HTTP request is validated.
Truncated, corrupt, oversized, aborted, and multiple-file uploads are rejected without
committing or rebooting. Only successful `Update.end(false)` causes reboot. CRC detects
accidental corruption; it does not authenticate the publisher or prove hardware compatibility.

## Offline checks

`tests/run_safety_tests.py` compiles the actual OTA handler with a mocked updater. It
checks valid images across several chunk sizes, renamed firmware, truncation, corruption,
extra bytes, invalid headers, filesystem data, multiple files, empty requests, aborted
uploads followed by retries, and begin/write/end failures. It also simulates the EEPROM power-cycle counter
(trigger, reset, clamp, no needless erase, stock bytes preserved). It checks that the
built image's FS constants match stock and that the image fits the OTA budget, and it
checks the storage/recovery invariants in the source. Actual AP access, button polarity, reset behavior,
flash preservation, and OTA boot must still be tested on recoverable hardware.
