# Device research

## Source listing
AliExpress item 1005011924447121 — "Smart weather clock electronic photo album weather
station animation display WiFi networking automatic update desktop ornament".
Store: Shop1105404926. Price ~AU$21. 4.6★, 1411 reviews, 10,000+ sold.

## Specifications (from listing)
- Size ~50 mm, weight ~100 g. "Is Smart Device: Yes."
- A "User manual (PDF)" is linked on the product page (worth downloading for completeness).

## This is a GeekMagic-class clone
The page's related/recommended items are **GeekMagic** units ("Super Cool Mini WiFi
Desktop Weather Clock", "smallTV"). This hardware family is ESP8266/ESP32 + ST7789 240×240
and has an active open-source firmware community. Useful references to check when extending
this project (verify licenses / exact board before borrowing):
- GeekMagic "smalltv" / "smalltv-pro" / "smalltv-ultra" community firmware.
- Various ESP8266 ST7789 weather-clock projects on GitHub.

## What reviews confirmed (matches our decomp)
- IPS panel; strong dimming ("black at min brightness practically OLED"); ~10 Hz refresh.
- Custom GIF animations are **80×80**.
- Setup flow: join the clock's WiFi AP → config web page → enter home WiFi + password →
  save & reboot → clock shows its IP on screen → browse to that IP → configure city,
  themes, colors, upload photos/GIFs. Ships with an English manual.

## Implications for our firmware
- Target ST7789 240×240, rotation 0, no offset (confirmed in [hardware.md](hardware.md)).
- Keep the AP-setup → IP-display → web-config flow (users expect it; implemented in main.cpp).
- Keep GIF/photo render at 80×80 for visual parity.
- Themes to match stock: Classic, Weather, Photo, Dial, Simple, Weather forecast, Flip Clock.
