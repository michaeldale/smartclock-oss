# Hardware profile

Reverse-engineered from the stock firmware (`SDPro_V1.0.6`) via Ghidra, cross-checked
against the AliExpress listing and buyer reviews. This is a GeekMagic-style "Smart
Weather Clock" clone.

## MCU / flash
- **ESP8266** (ESP-12), **4 MB** flash, DIO @ 40 MHz, Arduino core.
- Filesystem: **LittleFS** (web UI, photos, GIFs, config), **4m3m** layout: stock mounts
  0x40300000–0x405FA000 (flash 0x100000–0x3FA000), page 256 B, block 8 KB. Sketch +
  OTA staging share the first 1 MB. EEPROM sector at 0x3FB000.
- Stock also used EEPROM: weather key @0x6E (32 B), activation bytes @0x64 (6 B).

## Display — ST7789, 240×240 IPS, SPI, RGB565
- Confirmed by the init table (ST7789-specific regs B2/B7/BB/C0/C6/D0 + 14-byte E0/E1 gamma).
- `COLMOD 0x55` (16-bit), `INVON` (IPS inverted), reset pulse high→low 20 ms→high 150 ms.
- **Orientation:** stock boots `setRotation(0)` → `MADCTL 0x00`. For the 240×240 panel the
  driver's rotation handler falls through to the **default offset (0,0)** (the per-variant
  offsets only apply to 135/170/172/280 panels). MADCTL rotation table: `0x00 / 0x60 / 0xC0 / 0xA0`.
  → **No column/row offset needed. Rotation 0.**
- Reviews: IPS, very good dimming ("practically OLED" at min), ~**10 Hz** refresh.
- Custom GIF/photo render area is **80×80** (per reviews), centered in the 240×240 screen.

## Pin map (ESP8266 GPIO)
| Signal       | GPIO    | NodeMCU | Notes                                            |
|--------------|---------|---------|--------------------------------------------------|
| SPI SCLK     | GPIO14  | D5      | hardware HSPI                                    |
| SPI MOSI     | GPIO13  | D7      | hardware HSPI                                    |
| SPI MISO     | GPIO12  | D6      | unused by panel                                  |
| TFT CS       | GPIO15  | D8      | idle high                                        |
| TFT DC       | GPIO0   | D3      |                                                  |
| TFT RST      | GPIO2   | D4      | reset pulse at boot                              |
| Backlight    | GPIO5   | D1      | **PWM, range 1023, ACTIVE-LOW**                  |
|              |         |         | stock duty = `1023 - brightness*10`              |
| Button?      | GPIO4   | D2      | stock: `INPUT_PULLUP`, interrupt FALLING. No     |
|              |         |         | button found on the unit; never used for recovery|

### Backlight caution
Active-low: **low duty = bright**. If a rebuild drives it active-high the brightness
slider works backwards. Firmware here already inverts (`setBacklight()` in main.cpp).

### Strapping-pin caution (GPIO0/2/15)
CS=GPIO15, DC=GPIO0, RST=GPIO2 are all boot-strapping pins. The panel keeps them in the
required boot state (15 low, 0 high, 2 high) via its own pull resistors, but if you add
wiring, don't disturb boot levels or the ESP won't start / won't flash.

## Serial / recovery
The unit's USB tested **power-only** in this environment. To back up stock firmware or to
recover from a bad flash you need UART: a USB-TTL adapter on the ESP's TX/RX/GPIO0/GND/
3V3/EN pads (GPIO0→GND for flash mode). Establish this **before** flashing custom firmware.
