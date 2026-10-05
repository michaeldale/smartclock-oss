// Host stand-in for TFT_eSPI: draws into a 240x240 RGB565 framebuffer. Text uses
// the real TFT_eSPI font data and the same datum/padding rules; the anti-aliased
// primitives use a signed-distance approximation of TFT_eSPI's.
#pragma once
#include <Arduino.h>

enum { TL_DATUM = 0, TC_DATUM, TR_DATUM, ML_DATUM, MC_DATUM, MR_DATUM, BL_DATUM, BC_DATUM, BR_DATUM,
       L_BASELINE, C_BASELINE, R_BASELINE };

class TFT_eSPI {
 public:
  static constexpr int W = 240, H = 240;
  uint16_t fb[W * H] = {};

  void fillScreen(uint32_t c) { fillRect(0, 0, W, H, c); }
  void drawPixel(int32_t x, int32_t y, uint32_t c);
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c);
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t c) { fillRect(x, y, w, 1, c); }
  void fillCircle(int32_t x, int32_t y, int32_t r, uint32_t c);
  void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c);
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c);
  void fillSmoothCircle(int32_t x, int32_t y, int32_t r, uint32_t c, uint32_t bg = 0x00FFFFFF);
  void fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c, uint32_t bg = 0x00FFFFFF);
  void drawWedgeLine(float ax, float ay, float bx, float by, float ar, float br, uint32_t c, uint32_t bg = 0x00FFFFFF);
  void drawSmoothArc(int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t a0, uint32_t a1, uint32_t c, uint32_t bg, bool = false);
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data);
  void setSwapBytes(bool s) { swap = s; }

  void setViewport(int32_t x, int32_t y, int32_t w, int32_t h, bool = true) { vx = x; vy = y; vw = x + w; vh = y + h; }
  void resetViewport() { vx = vy = 0; vw = W; vh = H; }

  void setTextColor(uint16_t c) { fg = bgc = c; }
  void setTextColor(uint16_t c, uint16_t b) { fg = c; bgc = b; }
  void setTextDatum(uint8_t d) { datum = d; }
  void setTextPadding(uint16_t p) { padX = p; }
  int16_t textWidth(const char* s, uint8_t font);
  int16_t textWidth(const String& s, uint8_t font) { return textWidth(s.c_str(), font); }
  int16_t fontHeight(uint8_t font);
  int16_t drawString(const char* s, int32_t x, int32_t y, uint8_t font);
  int16_t drawString(const String& s, int32_t x, int32_t y, uint8_t font) { return drawString(s.c_str(), x, y, font); }

 private:
  int32_t vx = 0, vy = 0, vw = W, vh = H;
  uint16_t fg = 0xffff, bgc = 0;
  uint8_t datum = 0;
  uint16_t padX = 0;
  bool swap = false;
  void blend(int32_t x, int32_t y, float a, uint32_t c, uint32_t bg);
  int16_t drawChar(uint16_t ch, int32_t x, int32_t y, uint8_t font);
};
