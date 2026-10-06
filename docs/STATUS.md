# Current status and next work

Updated: 2026-10-05. Update this page when a release, device result or next
step changes; keep the dated [decision records](decisions/README.md) as the
history.

## Release and checkout

The latest release is **1.2.0** (2026-10-06, tag `v1.2.0`), published on GitHub as
`michaeldale/smartclock-oss` with `SDPro_SmartClockOSS_v1.2.0.bin` built by
`scripts/make_release.py` (no WiFi details). It adds the integrations (custom faces,
geekmagic-hacs, MQTT), all off by default. The test unit at 10.0.1.118 runs the same
code. Earlier: 0.1.2 added the device password; 0.1.1 was the first public release;
0.1.0 (`v0.1.0`, `6d94d08`) was the first image flashed over stock SD Pro V1.0.6
([record](decisions/2026-10-05-first-flash-over-stock.md)).

## What has been shown on the device

| Feature | Device result |
|---|---|
| OTA over stock through the stock page | Works, with the image renamed to start with `SDP` |
| Stock LittleFS volume mounts (4m3m layout) | Yes: `storageReady` true, not in recovery |
| Build-time WiFi from `secrets.ini` | Joined the network on first boot |
| Power-cycle recovery | Enters recovery as intended |
| Local time | Correct: `/status` reported `21:54:26 AEDT` matching local time ([issue](issues/2026-10-05-clock-one-hour-behind-during-dst.md)) |
| mDNS name | Workstation is on another subnet, so `.local` cannot resolve there ([issue](issues/2026-10-05-smartclock-local-not-resolving-on-windows.md)). The checkout uses `smartclock-<chip id>` and sets it as the DHCP hostname |
| Open-Meteo weather, reset-reason power cycles, heap gates | Flashed (`016451e`): geocoding resolved "Sydney, New South Wales, AU" at boot, but the forecast fetches in the first ~2 min failed as "unreachable"; a manual sync then succeeded in 1.1 s with 4 forecast days. The checkout waits 5 s after the link comes up, uses a 10 s timeout and reports the client error code ([record](decisions/2026-10-05-open-meteo-reset-reason-and-discovery.md)) |
| Hostname | `smartclock-adf20d`; last reset reported as "Software/System restart" after OTA |
| Our own `/update_ota` | Works: 0.1.0 to 0.1.1-dev |
| New settings page | Flashed; served gzipped (9,942 B), `/status` reports live state |
| Seven clock faces, forecast, photos | Flashed (`4252007`). Live: current weather (light rain, 18.3 C), 4 forecast days, free heap 32 KB. Look on the panel not yet reported ([record](decisions/2026-10-05-clock-faces-without-a-framebuffer.md)) |
| Stock photos | All removed on request: `1.jpg` (replaced by a generic `dusk.jpg`), then `like.gif` and `space man.gif` once `016451e` could delete any file. `space man.gif` showed broken because `/photo/` did not decode `%20`; fixed in the checkout |
| Crash-marker recovery, format storage | Offline tests only |
| 1.2.0: custom faces, geekmagic-hacs API, MQTT (all off by default) | Flashed. The geekmagic-hacs SD_PRO sequence, replayed without sign-in (detect, upload, exclusive photo and face, 1 s interval, theme=2), showed the test dashboard; settings, WiFi and firmware stayed refused. The 1 s interval made the Photo face re-decode and blink once a second; fixed in the checkout (no redraw of the same photo, no clear before a full-screen photo). MQTT and a real Home Assistant not yet tried ([record](decisions/2026-10-06-integrations.md)) |
| 0.1.2: device password, update banner, sunset night mode, backup/restore | Flashed: sign-in, Show on clock (hidden again on sign-in) and the build ID confirmed on the device ([record](decisions/2026-10-06-passwords-and-flash-savings.md)) |

## Offline checks

`python tests/run_safety_tests.py` after `pio run`: the OTA handler against a
mocked updater, the power-cycle counter against a simulated EEPROM, the image's
LittleFS constants against stock, the size budget, and source invariants.

`python tests/face_preview/run_face_preview.py`: every face rendered to PNG in
`.pio/face-preview/` (with `sheet.png`), and incremental redraws compared with
full redraws.

## Flash budget

The checkout's image is 502,416 bytes, 97% of the 0x7E000 OTA limit, leaving about
14 KB, with the integrations ([record](decisions/2026-10-06-integrations.md)). mDNS
(19.9 KB) is the next lever if space runs out.
Measure any addition before it lands; the build refuses an image over the limit.

## Next

1. Then the [roadmap](plans/roadmap.md) candidates: a way back to stock, CI, crash reports.

## Open issues

See [issues](issues/README.md).
