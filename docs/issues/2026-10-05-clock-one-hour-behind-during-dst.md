# The clock is an hour behind during daylight saving

Status: Fixed in 0.1.1-dev; flashed 2026-10-05, `tz` rule active on the device.

Date: 2026-10-05

## Summary

On 0.1.0 the clock at 10.0.1.118 showed the time one hour behind. Sydney moved
from AEST (+10:00) to AEDT (+11:00) on Sunday 2026-10-04.

## Cause

`loop()` called `configTime(cfg.timezoneMin * 60, 0, cfg.ntp.c_str())`: a
fixed +600-minute offset with a DST offset of 0. Nothing ever applied daylight
saving. The stock `timezone` setting has the same shape (a minute offset), so
copying it carried the limitation over.

## Fix

`Config` gains `tz`, a POSIX TZ rule, defaulting to
`AEST-10AEDT,M10.1.0,M4.1.0/3` (AEDT from the first Sunday in October to the
first Sunday in April, at 03:00 local). `applyTime()` uses
`configTime(tz, ntp)` when `tz` is set, otherwise the fixed offset.

- `/api/set?key=tz` sets the rule (up to 63 characters) and applies it at once.
- `/api/set?key=timezone` still works and clears `tz`, so a stock client gets
  the fixed offset it asked for.
- `/config` reports `tz`, and the web page has a Timezone list.

A device with no `/config.json` picks up the new default on its first boot of
0.1.1. A saved `/config.json` without `tz` also gets the default, because
`load()` keeps the default for missing keys.

## To verify

After flashing 0.1.1: the displayed time matches local AEDT, and `/config`
shows the `tz` rule.

## 2026-10-05: on the device

0.1.1-dev was flashed through our own updater. `/config` on the device reports
`"tz":"AEST-10AEDT,M10.1.0,M4.1.0/3"`
([record](../decisions/2026-10-05-first-update-through-own-ota.md)). The
settings page now shows the device's local time with its zone abbreviation
(AEDT during daylight saving), which is the remaining visual check.

With the settings-page build flashed, `/status` reported
`"time":"2026-10-05 21:54:26 AEDT"`, matching local time. Verified.
