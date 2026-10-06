# Optional integrations: custom faces, geekmagic-hacs, MQTT

Date: 2026-10-06

Prompted by research into ESPHome on these clocks, AWTRIX 3's custom apps and the
geekmagic-hacs Home Assistant integration. The API is in
[integrations-api.md](../specifications/integrations-api.md).

## What the research showed

- **ESPHome** on the GeekMagic SmallTV/Ultra/Pro needs a UART first flash. On the
  ESP8266, a 240×240 frame buffer does not fit, so it relies on a no-buffer ST7789
  driver fork with about 2.5 s per full refresh, and its display pages do not work
  there. Faces are compiled into the YAML. What it really adds is the Home Assistant
  connection.
- **AWTRIX 3** custom apps are pushed as JSON over HTTP/MQTT (text, icon, charts,
  progress, draw commands), with `lifetime` and an empty payload to delete. All the
  logic stays in the smart home.
- **geekmagic-hacs** renders dashboards in Home Assistant (18 widgets, 19 layouts)
  and pushes JPEGs. It already speaks the stock **SD Pro** photo API, which it
  detects by `GET /theme/list` returning `{"themes":[...]}`. It sends no credentials.

## Decisions

- **All three are off by default, each with its own switch.** They open different
  doors: the stock-compatible API works without sign-in, and MQTT talks to a broker.
- **Custom faces** follow AWTRIX's model on a colour screen. Items cover text, rect,
  circle, line, ring, bar, weather icon and image. **Placeholders** (`{time}`,
  `{temp}`, ...) are resolved on the clock, and **variables** are pushed separately,
  so live values need no re-sent layout. Faces are files in `/faces`; variables live
  in RAM. Access is a signed-in session or a generated **API key** (`X-Api-Key`).
- **Redraws without a frame buffer:** text with `bg`, rings, bars and icons with
  `bg` redraw alone when their value changes; anything else redraws the face. The
  face preview's incremental check covers two custom faces, including a variable
  change and a weather update.
- **The stock-compatible API** answers only while the switch is on, or for a
  signed-in page (the Photos card's slideshow switches use it). Without sign-in it
  reaches only photos, the face choice and brightness (`/api/set` keys `theme`,
  `lcd_brightness`, `brightness`). The safety tests check each guard.
- **MQTT discovery** publishes a light (backlight), a select (face), four sensors
  and a button. Discovery JSON is built as strings (every value is fixed, a
  sanitised name or a number). A missing broker is retried every 30 s with a 3 s
  socket timeout, so it does not stall the clock for long.
- Photos gained per-photo slideshow switches and a seconds-per-photo setting,
  shared by the stock API and the settings page.

## Size

The three integrations, PubSubClient and the UI cost about 30 KB. ArduinoJson
without 64-bit integers or doubles (`ARDUINOJSON_USE_LONG_LONG=0`,
`ARDUINOJSON_USE_DOUBLE=0`) won back 4.4 KB. The image is 502,416 bytes, 97% of
0x7E000, leaving 13.7 KB. Static RAM is 43.0 KB (+2.3 KB, mostly the custom-face
document and the variables table). The largest optional piece left is mDNS
(19.9 KB); dropping it is the next lever if space runs out.

## Not shown on hardware yet

Faces were rendered with the real fonts on the PC, and the settings page was driven
against a mock. MQTT discovery, the geekmagic-hacs flow and photo uploads from Home
Assistant have not run against a real broker, Home Assistant or the device.
