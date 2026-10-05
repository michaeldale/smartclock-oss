#pragma once
// Pin map reverse-engineered from stock firmware (SDPro_V1.0.6). See docs/specifications/hardware.md.
// ESP8266 GPIO numbers (NodeMCU Dx labels in comments).

// ---- ST7789 240x240 SPI (hardware HSPI) ----
#define PIN_TFT_SCLK   14   // D5  (HSPI CLK)
#define PIN_TFT_MOSI   13   // D7  (HSPI MOSI)
#define PIN_TFT_CS     15   // D8
#define PIN_TFT_DC      0   // D3
#define PIN_TFT_RST     2   // D4

// ---- Backlight: PWM, ACTIVE-LOW. duty = PWM_RANGE - brightness*scale ----
#define PIN_BACKLIGHT   5   // D1
#define BL_PWM_RANGE 1023   // stock uses analogWriteRange(1023)
// Stock maps: analogWrite(5, 1023 - brightnessPercentApprox*10)

// ---- Button: active-low, INPUT_PULLUP, interrupt on FALLING ----
#define PIN_BUTTON      4   // D2

// Display geometry
#define TFT_W 240
#define TFT_H 240
// Stock boots setRotation(0) -> MADCTL 0x00, column/row offset (0,0) for the 240x240 panel.
// MADCTL rotation table (from decomp): rot0=0x00 rot1=0x60 rot2=0xC0 rot3=0xA0
