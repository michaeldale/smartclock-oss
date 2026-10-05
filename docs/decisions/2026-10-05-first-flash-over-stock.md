# First flash over stock SD Pro V1.0.6

Date: 2026-10-05

## What was flashed

- Source: commit `6d94d08` (tag `v0.1.0`), with build-time WiFi from
  `secrets.ini`.
- Image: 444,528 bytes, SHA-256
  `0f3405c9f552bf4d31d6779a1dc0463739cf3cad9a1432f9aa5de5e98f9cd834`.
  This differs from a build without `secrets.ini` only by the WiFi strings.
- Uploaded through the stock firmware's own update page, as
  `SDPro_SmartClockOSS.bin`.

## The stock page's filename check

The first upload, named `firmware.bin`, was refused with "Please use the
correct firmware". The stock page's `javascript.js` (served from the stock
LittleFS volume, not from the firmware image) contains:

```js
if (!file.name.startsWith("SDP")) { alert("Please use the correct firmware"); return; }
```

It checks only the name, in the browser. Renaming the file was enough. The
stock page posts the file to `/update_ota` as field `update`.

## Result

The upload succeeded and the clock rebooted into our firmware, joined the
build-time network, and answered at http://10.0.1.118/:

```
/version  SmartClock-OSS V0.1.0
/status   {"recovery":false,"storageReady":true,"storageMountFailed":false}
/config   defaults (no /config.json saved yet), timezone 600
```

The stock LittleFS volume mounted, confirming the
[layout decision](2026-10-05-flash-layout-matches-stock.md). Power-cycle
recovery then worked on the device
([record](2026-10-05-recovery-without-a-button.md)).

## Found afterwards

- The time was an hour behind
  ([issue](../issues/2026-10-05-clock-one-hour-behind-during-dst.md)).
- `smartclock.local` did not resolve from `ping` on Windows
  ([issue](../issues/2026-10-05-smartclock-local-not-resolving-on-windows.md)).
- Our own `/update_ota` has not yet been used on the device. The next update
  will be its first run.
