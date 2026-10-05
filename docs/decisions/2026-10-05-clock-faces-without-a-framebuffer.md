# Seven clock faces without a framebuffer

Date: 2026-10-05

## Decision

All seven stock face ids are implemented in `src/faces.cpp`: Classic (0),
Weather (1), Photo (2), Dial (3), Simple (4), Forecast (5) and Flip (6). Each
face draws only the regions whose content changed, never from a full-screen
buffer. JPEGs are decoded with the `tjpgd` core directly, not through the
`TJpg_Decoder` wrapper. A host-side preview (`tests/face_preview`) renders
every face and checks the incremental drawing.

## Why no framebuffer

A 240x240 RGB565 buffer is 115 KB; even 4 bpp is 28.8 KB against about 34 KB
of free heap. So each face keeps the last values it drew (minute, second, day,
weather version) and redraws only those regions. Text is drawn with a background
colour or padding so it overwrites the old text. The Dial erases the previous
hands by drawing the same anti-aliased wedges, slightly wider, in the background
colour, then redraws the date and temperature they sweep across, then the hands.

`tests/face_preview/run_face_preview.py` steps each face second by second for
150 s, across minute and hour-width changes and a weather update. It then
compares the result pixel by pixel with a full redraw at the same moment. All
six non-photo faces match exactly. The Photo face is excluded because it changes
photo on a timer by design.

## Why tjpgd directly

The first build with photos was 544,048 bytes, 28 KB over the 0x7E000 OTA
budget, and the size guard refused it. 32.6 KB of that was SPIFFS:
`TJpg_Decoder.cpp` references the `SPIFFS` object, which pulls the whole SPIFFS
implementation into a LittleFS-only firmware. Calling `jd_prepare`/`jd_decomp`
with our own LittleFS reader removes it. It also replaces the wrapper's
permanent 3.6 KB workspace with a buffer allocated only while decoding.
GLCD and font 7 were dropped as unused; fonts 6 and 8 were added.

The result is 490,544 bytes (95% of the budget) and 39.5 KB of static RAM. About
25 KB is left for future work.

## Found by the preview

- This `tjpgd` copy keeps a `swap` byte-order flag across `jd_prepare()`. An
  uninitialised `JDEC` gave random byte order: the test photo's yellow came out
  blue. `JDEC` is now zero-initialised with `swap = 0`.
- With a one-digit hour, the big time was off-centre. The group is now centred,
  and its band is cleared only when the width changes.

## Not shown on hardware yet

The preview uses the real TFT_eSPI font data and datum/padding rules, but its
anti-aliasing approximates TFT_eSPI's, and it does not model SPI speed. The flip
animation timing, JPEG decode time and the look on the IPS panel still need the
device.
