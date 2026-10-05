# The flash layout matches stock: 4m3m

Date: 2026-10-05

## Decision

Build with `eagle.flash.4m3m.ld`, the layout the stock firmware uses, and fail
the build when `firmware.bin` exceeds 0x7E000 bytes.

## Evidence

Both stock images (`SDPro_V1.0.4`, `SDPro_V1.0.6`) carry this literal pool
beside their LittleFS constructor:

```
40300000 bfe00000 405fa000 00000100 00002000
```

That is `_FS_start` 0x40300000, `_FS_end` 0x405FA000, page 0x100 and block
0x2000: the 4m3m layout, a 3 MB filesystem at flash offset 0x100000. The
EEPROM sector constant 0x405FB000 is also present.

The 0.1.0 image carries `40300000 405fa000 00002000 00000100`, the same
values. `tests/run_safety_tests.py` checks this on every run.

## Why

The scaffold used 4m2m, which puts the filesystem at 0x200000. After an OTA
over stock it would have looked in the middle of the stock volume, failed to
mount, and stayed in recovery permanently, because the firmware never formats
automatically. Its own later updates would also have staged into the stock
volume.

On 4m3m the first 1 MB holds the running sketch and the staged update, so an
image must fit twice. The updater needs `roundup(new) <= 0x100000 -
roundup(current)`, with our one-sector margin. 0x7E000 keeps a sector of slack.
The first flash through the stock updater allows about 0x87000 (1 MB minus the
0x78000 stock image).

## Result

Confirmed on the device on 2026-10-05: `/status` reported
`{"recovery":false,"storageReady":true,"storageMountFailed":false}`
([first flash](2026-10-05-first-flash-over-stock.md)).
