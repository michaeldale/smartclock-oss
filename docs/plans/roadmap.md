# Roadmap after 0.1.1

Date: 2026-10-05

Status: Open; not scheduled.

Every item has to fit the OTA size budget: 0x7E000 bytes. With all seven faces
and Open-Meteo the image is at 96% (about 19 KB left). Measure each addition
before it lands.

## Security

- Authentication for mutating endpoints and/or a WPA2 AP password
  ([issue](../issues/2026-10-05-ap-and-ota-unauthenticated.md)).

## Themes and display

- Done in 0.1.1: all seven faces, photos, rotation set, immediate redraw on
  settings changes.
- GIF playback (AnimatedGIF) in an 80x80 area, if it fits the remaining budget.
- Check whether the stock volume has photos or GIFs worth showing.

## Stock compatibility

- Stock-shaped `/photo/*` and `/theme/*` endpoints, if a stock client needs them
  (ours are under `/api/photos`).
- Import the WiFi details stock kept in EEPROM, if their layout can be
  recovered from the decompile.

## Tooling

- Build the image with an `SDP` filename prefix as well, so it can go through
  the stock page without renaming.
