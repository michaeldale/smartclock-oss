# Roadmap after 0.1.1

Date: 2026-10-05

Status: Open; not scheduled.

Every item has to fit the OTA size budget: 0x7E000 bytes. With all seven faces
and Open-Meteo the image was at 96%; 0.1.2 freed 34 KB and is at 92% (about
43 KB left). Measure each addition before it lands. mDNS (19.9 KB) is the next
thing to drop if space runs out.

## Security

- Done in 0.1.2: device password for the setup network and settings page.

## Next candidates

- A way back to stock over WiFi: accept a stock image whose SHA-256 matches a
  known vendor release (V1.0.4 / V1.0.6).
- GitHub Actions: build, test and publish release images from the public source.
- Crash reports: keep the last exception's cause and address across the reboot
  and show them on the System card.

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
