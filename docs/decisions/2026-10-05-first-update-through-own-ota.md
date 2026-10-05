# First update through our own updater: 0.1.0 to 0.1.1-dev

Date: 2026-10-05

## What was flashed

0.1.1-dev from the working tree after `v0.1.0` (DST fix and Timezone setting),
built with `secrets.ini`. It was uploaded from the clock's own **Firmware
Update** card at http://10.0.1.118/, so our `/update_ota` handler ran on
hardware for the first time.

## Result

The update was reported as working, and the clock answered afterwards:

```
/version  SmartClock-OSS V0.1.1-dev
/status   {"recovery":false,"storageReady":true,"storageMountFailed":false}
/config   ... "timezone":600,"tz":"AEST-10AEDT,M10.1.0,M4.1.0/3" ...
```

- The validated, withheld-final-byte update path commits and reboots on
  hardware, not only against the mock updater.
- The 0.1.1 image fitted the staging space left by the running 0.1.0 image,
  as the size budget predicts
  ([layout record](2026-10-05-flash-layout-matches-stock.md)).
- The default `tz` rule was picked up without a saved `/config.json`
  ([DST issue](../issues/2026-10-05-clock-one-hour-behind-during-dst.md)).

The displayed time was not separately reported in this result. The settings
page added after this record shows the device's local time and zone
abbreviation, which makes that check direct.
