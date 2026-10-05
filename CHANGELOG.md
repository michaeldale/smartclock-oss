# Changelog

All notable SmartClock-OSS changes are recorded here. Versions follow semantic
versioning. `/version` on the device reports the version it was built as; a
`-dev` suffix marks a build from an unreleased checkout.

## 0.1.1 - 2026-10-06

### Fixed

- **The clock showed standard time during daylight saving, an hour behind.**
  The time was set with a fixed minute offset (`timezone`, +600 for Sydney)
  and no DST rule, so it stayed on AEST after Sydney moved to AEDT on
  2026-10-04. The time zone is now a POSIX TZ rule (`tz`), so libc switches
  at each DST change. The default is `AEST-10AEDT,M10.1.0,M4.1.0/3`
  ([issue](docs/issues/2026-10-05-clock-one-hour-behind-during-dst.md)).
  Flashed through our own updater; the rule is active on the device
  ([record](docs/decisions/2026-10-05-first-update-through-own-ota.md)).
- Numeric settings are range-checked (theme, brightness, hours, interval).

### Removed

- **OpenWeatherMap.** Open-Meteo replaces it entirely, saving ~5 KB of flash
  that the OTA budget needs. The `weatherkey` setting (stock's too) is no
  longer accepted, and a saved key is dropped at the next settings save.
- Only power-on and external resets count toward the three-power-cycle recovery
  gesture; crashes, watchdogs and restarts clear it.
- mDNS starts after the network connects and only with 28 KB of free heap; a
  weather fetch waits while free heap is under 16 KB.
- Weather was not fetched until 10 minutes after boot; it is now fetched at once.
- The city is URL-encoded in weather requests (a space broke them).
- `/api/photos/delete` removes any listed file in `/photo`, so files the stock
  firmware left (GIFs, names with spaces) can be deleted. The page marks
  non-JPEG files "Not shown".
- Photos whose names contain spaces or other URL-encoded characters showed as
  broken thumbnails: `/photo/` was served by `serveStatic()`, which matches the
  raw URI. A dedicated handler now decodes the name and serves only files
  directly in `/photo`.

### Added

- **Weather without an API key, from Open-Meteo.** The city ("Sydney" or
  "Sydney, AU") is geocoded once and cached, and one ~1 KB request every 15
  minutes gives current conditions and the 4-day forecast
  ([record](docs/decisions/2026-10-05-open-meteo-reset-reason-and-discovery.md)).
- **Sync now** on the Weather card (`POST /api/weather/sync`, at most once per
  10 s) fetches immediately, reports the result, and restarts the 15-minute
  timer; the card shows when the weather last updated (`weatherAge`).
- **A per-unit hostname**, `smartclock-<chip id>`, used for mDNS and as the DHCP
  hostname so routers list the clock by name. mDNS TXT records give model,
  version, board, API and path.
- `/status` reports `resetReason`, `heapLow`, `weatherSource`, `weatherError`
  and the resolved `location`.
- **All seven clock faces, redesigned.** Classic (date, large time, seconds bar,
  weather line), Dial (anti-aliased hands and ticks, date and temperature),
  Weather (large icon and temperature, high/low, feels-like, humidity, wind),
  Forecast (four day cards with icon, high/low and chance of rain), Flip
  (split-flap cards with a fold animation), Simple (the time and date) and
  Photo (your photos with the time on top). Weather icons are drawn, not
  stored, at any size: sun, moon, clouds, rain, storm, snow and mist. A
  "Setting the time" screen replaces 1970 before NTP. Faces redraw only what
  changed ([record](docs/decisions/2026-10-05-clock-faces-without-a-framebuffer.md)).
- **Rotation that skips empty faces.** Rotation and the button move to the
  next face that has something to show, from a chosen set (`faces` bitmask,
  default all). A rotated face is not saved, so the chosen face stays yours.
- **4-day forecast** with daily high, low, icon and chance of rain. A failed
  fetch retries after a minute instead of waiting the full interval.
- **Photos.** `/api/photos` (list and storage use), `/api/photos/upload`
  (JPEG, up to 200 KB, keeps 128 KB free) and `/api/photos/delete`. The
  settings page crops and resizes photos to 240x240 in the browser before
  upload, so a phone photo arrives as a ~10-30 KB baseline JPEG.
- **Wind in km/h or mph** (`mile`), and new default colours (amber, soft blue,
  mint) for units with no saved colours.
- **`tests/face_preview`** renders every face on the PC with the real fonts and
  JPEG decoder, and checks incremental redraws against full redraws.
- **A new settings page.** Overview tiles (device time with zone, network and
  signal, weather, uptime), face picker, brightness and colour pickers, night
  mode, time and region, weather, a WiFi scan with signal strength, a
  drag-and-drop firmware update with progress, and system info. Changes save
  as you make them. Light and dark themes, phone to desktop width, and no
  external resources, so it works on the offline setup network. Recovery mode
  locks the settings and says why. The page lives in `web/index.html` and is
  gzipped into flash at build time (about 30 KB to 10 KB).
- `/status` reports version, uptime, free memory, network, local time,
  the resolved weather location, and the latest weather. It never includes
  secrets.
- `/scanwifi?detail=1` adds signal strength and security; without it the
  response keeps the stock shape.
- `/api/set` accepts `hour12`; setting the city fetches
  weather at once.
- `/api/set?key=tz&value=<POSIX TZ>` and a **Timezone** list on the web page.
  `/config` reports `tz`. The stock `timezone` key still works and sets a
  fixed offset with no DST, as stock did. Changing `tz`, `timezone` or `ntp`
  takes effect without a reboot.

## 0.1.0 - 2026-10-05

The first release, and the first firmware flashed over the stock SD Pro
V1.0.6 firmware. Tag `v0.1.0`, commit `6d94d08`. Flashed through the stock
update page on the unit at 10.0.1.118, where it came up in normal mode with
the stock LittleFS volume mounted
([record](docs/decisions/2026-10-05-first-flash-over-stock.md)).

### Flashing over stock

- **The flash layout matches stock (4m3m).** Both stock images mount LittleFS
  at `0x40300000`-`0x405FA000` with 256 B pages and 8 KB blocks. The image
  carries the same constants, so the stock volume mounts after OTA
  ([record](docs/decisions/2026-10-05-flash-layout-matches-stock.md)).
- **OTA size budget.** Sketch and staged update share 1 MB, so the build fails
  above 0x7E000 bytes and deletes the image. 0.1.0 is 444,496 bytes (86%).
  Unused TFT fonts were dropped to make room.
- **The stock update page only accepts filenames starting with `SDP`.** It is
  a browser-side check; renaming the image is enough. See
  [MIGRATION.md](docs/MIGRATION.md).

### Recovery without a button

- **Three quick power cycles enter recovery** (on for less than 10 s twice,
  then leave it on). The count lives in the EEPROM sector, and stock bytes
  there are preserved. Confirmed working on the device
  ([record](docs/decisions/2026-10-05-recovery-without-a-button.md)).
- A crash in the first 30 s, or a filesystem that will not mount, also enters
  recovery. Every deliberate reboot clears both triggers (`safeRestart()`).
- Recovery shows its state on the panel, and offers an explicit **Format
  storage** only after a mount has failed. Nothing formats automatically.
- GPIO4 is never read to choose recovery.

### OTA validation

- Firmware-only OTA: header, declared length and whole-image CRC are checked
  before the final byte is written. Truncated, corrupt, oversized, aborted and
  multi-file uploads are rejected without a reboot.

### Networking

- The setup AP, **Smart Weather Clock** at http://192.168.4.1/, is always on.
  The STA side retries in 30 s windows every 5 minutes so the AP stays usable
  when the saved network is missing.
- Optional build-time WiFi from a gitignored `secrets.ini`, used only when
  nothing is saved, and in recovery.
- mDNS as `smartclock.local`.

### Clock

- Classic and Dial themes, NTP, OpenWeatherMap, night dimming, and the
  stock-compatible web API (`/config`, `/api/set`, `/scanwifi`, `/connect`,
  `/restart`, `/version`, `/update_ota`). `/config` does not expose WiFi or
  API secrets, unlike stock.

### Known issues

- The time is an hour behind during daylight saving (fixed in 0.1.1).
- The AP is open and OTA is unauthenticated
  ([issue](docs/issues/2026-10-05-ap-and-ota-unauthenticated.md)).
- You cannot OTA back to the stock firmware; that needs UART.
