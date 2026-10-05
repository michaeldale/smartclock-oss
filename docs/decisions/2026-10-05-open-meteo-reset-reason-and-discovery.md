# Open-Meteo for weather, power cycles by reset reason, and discovery

Date: 2026-10-05

Adopted from a review of the sibling `geekmagic-tv-esp8266` firmware (SmallTV
Ultra). This record supersedes the crash-loop part of
[recovery without a button](2026-10-05-recovery-without-a-button.md).

## 1. Open-Meteo is the only weather source

Open-Meteo needs no key and works over plain HTTP:

- **Geocoding:** `geocoding-api.open-meteo.com/v1/search?name=<city>&count=1`,
  with `&countryCode=XX` when the city is written "Sydney, AU". It runs only
  when the city changes. The result is cached in the config as `lat5`/`lon5`
  (integer 1e-5 degrees), `geoname` and `geofor`, so there is one flash write
  per city change. Results carry long postcode lists, so the parse is filtered.
- **Weather:** one request to `api.open-meteo.com/v1/forecast` returns current
  conditions and 4 daily entries. On 2026-10-05 for Sydney it was 948 bytes,
  against ~16 KB for OpenWeatherMap's forecast. Fetched every 15 minutes, the
  model's current-data interval.
- WMO weather codes are mapped to OpenWeatherMap icon codes, so the faces are
  unchanged. `is_day` picks the day or night icon.

OpenWeatherMap was first kept as an option, then removed on request to save
flash: its two fetch paths, the 16 KB forecast filter and the key setting came to
~5 KB. `/api/set?key=weatherkey` now answers "Unknown Key", and a saved key is
dropped at the next settings save.

## 3. Only real power cycles count

`setup()` reads `ESP.getResetInfoPtr()->reason`. Only `REASON_DEFAULT_RST`
(power-on) and `REASON_EXT_SYS_RST` count toward the three-power-cycle gesture.
Any other reset clears the count: exceptions, watchdogs, and software restarts
(including the one after OTA). A crash loop no longer passes for the gesture.
Startup crashes are still caught by the RTC marker. `/status` reports
`resetReason`.

## 4. Heap gates

- mDNS starts only once the station is connected, 1.5 s have passed since
  boot, and free heap is at least 28 KB (the responder allocates on start).
  Before this it started in `setup()`, before WiFi.
- A weather fetch is postponed by 30 s while free heap is under 16 KB.
- `/status` reports `heapLow`, the lowest free heap seen. `getMaxFreeBlockSize()`
  was tried and dropped: it adds 1.6 KB of umm statistics code.

## 6. Discovery

- The hostname is `smartclock-<last 6 hex digits of the chip id>`, unique per
  unit. It is used for mDNS and as the **DHCP hostname** (`WiFi.hostname()`
  before the station starts), so a router can list or resolve the clock by
  name. mDNS is link-local multicast and does not cross subnets. That is why
  `smartclock.local` never resolved from the workstation, which sits on a
  different subnet from 10.0.1.118.
- The `_http._tcp` service carries TXT records `model`, `version`, `board`,
  `api` and `path`.

## Size

With both providers the image reached 502,192 bytes (97%). Measured against
the previous commit by symbol: `fetchOpenMeteo` ~4 KB, config fields ~1.4 KB,
mDNS TXT ~0.6 KB, the rest spread out. Removing OpenWeatherMap brought it to
497,216 bytes (96%), static RAM 40.2 KB. Moving the WMO table to code
with `F()` strings saved 0.9 KB of RAM. Replacing `sscanf`/`mktime`/`String(float)`
saved little: `sscanf` was already linked by other code. About 19 KB of
headroom remains.

## Not shown on hardware yet

Geocoding and the forecast request were checked against the live API from the
workstation. The weekday arithmetic was checked against Python for 2000-2099.
None of it has run on the device.
