# Stock firmware analysis (decompilation notes)

Static analysis of the stock `SDPro_V1.0.6` image (ESP8266, Arduino + Espressif NONOS SDK).
Done with Ghidra 12.1.3, language `Xtensa:LE:32`. The app image was split into its segments
(IROM @0x40201010 + IRAM/DRAM) and ~2,696 functions were decompiled. Key findings below;
these drove the pin map and the display driver config.

## Display driver
- Adafruit-GFX-style init-table interpreter (`FUN_4021a598`): `count`, then per entry
  `cmd, argCount|0x80-delay, args…, [delay]`. `pgm_read_byte` = `FUN_4022f5a8`.
- Init table @0x4026a308 decodes to the canonical **ST7789 240×240** sequence
  (SLPOUT, COLMOD 0x55, MADCTL 0x00, CASET/RASET 0..239, PORCTRL, GCTRL, VCOMS, power,
  gamma E0/E1, INVON, NORON, DISPON).
- `setRotation` (`FUN_4021a3a0`): offsets (0,0) for 240×240; MADCTL `0x00/0x60/0xC0/0xA0`.
- Reset (`FUN_4021a648`): RST high → low 20 ms → high → 150 ms.

## GPIO
- `pinMode` = `FUN_40221dac` (note GPIO16 special case), `digitalWrite` = `PTR_FUN_40209b78`,
  `analogWrite` = `FUN_40222274`, `attachInterrupt` = `FUN_402221f0`.
- Derived map: SCLK=14, MOSI=13, CS=15, DC=0, RST=2, backlight=5 (PWM active-low), button=4.
- Brightness: `analogWrite(5, 1023 - value*10)`.

## HTTP API (routes present in the binary)
`/`, `/index.html`, `/indexkey.html`, `/indexwifi.html`, `/photo-setting.html`,
`/config`, `/api/set`, `/submit`, `/scanwifi`, `/connect`, `/restart`, `/mac`,
`/update_ota`, `/theme/{list,toggle,interval}`, `/photo/{list,upload,delete,toggle,interval}`,
plus static assets (`/style.css`, `/javascript.js`, `/weather/`, `/ico*`, gifs/jpgs).
`/api/set` keys: city, ntp, weatherkey, timezone, theme, celsius, mile, timeformat,
color1/2/3, lcd_brightness, nightmode, starttime, stoptime, nightbrightness, ssid, password.

## Weather / time
- OpenWeatherMap over **plain HTTP** (`/data/2.5/weather` + `/forecast`, `?q=` or `?id=`).
- NTP (`pool.ntp.org`), resync every 30 min, timezone offset in minutes.

## Security issues in stock (do NOT reproduce)
1. `/config` returns **WiFi password + API key in cleartext**, unauthenticated.
2. No auth on `/api/set`, `/restart`, `/update_ota` — any LAN host can reconfigure/reflash.
3. Local "activation": licence = CRC-8 (poly 0x07, init 0xCD) over the MAC, 12-char compare
   (`FUN_40207d84`). Cryptographically meaningless; noted only as a finding. Not reimplemented.

Our firmware omits all three: no secrets via `/config`, and no fake activation gate.
Adding auth to mutating endpoints is a TODO.
