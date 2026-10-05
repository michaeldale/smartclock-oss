#include "TFT_eSPI.h"

// The real font tables (Font16.c etc. expect PROGMEM and these includes).
#include <Fonts/Font16.c>
#include <Fonts/Font32rle.c>
#include <Fonts/Font64rle.c>
#include <Fonts/Font72rle.c>

namespace {
struct FontInfo { const unsigned char* const* chars; const unsigned char* widths; uint8_t height, baseline; };
FontInfo fontInfo(uint8_t f) {
  switch (f) {
    case 2: return {chrtbl_f16, widtbl_f16, 16, 13};
    case 4: return {chrtbl_f32, widtbl_f32, 26, 19};
    case 6: return {chrtbl_f64, widtbl_f64, 48, 36};
    case 8: return {chrtbl_f72, widtbl_f72, 75, 73};
    default: return {nullptr, nullptr, 8, 7};
  }
}
float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
}  // namespace

void TFT_eSPI::drawPixel(int32_t x, int32_t y, uint32_t c) {
  if (x < vx || y < vy || x >= vw || y >= vh) return;
  fb[y * W + x] = c;
}

void TFT_eSPI::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  for (int32_t j = y; j < y + h; ++j)
    for (int32_t i = x; i < x + w; ++i) drawPixel(i, j, c);
}

void TFT_eSPI::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c) {
  for (int32_t y = -r; y <= r; ++y)
    for (int32_t x = -r; x <= r; ++x)
      if (x * x + y * y <= r * r + r) drawPixel(x0 + x, y0 + y, c);
}

void TFT_eSPI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c) {
  for (int32_t j = 0; j < h; ++j)
    for (int32_t i = 0; i < w; ++i) {
      int32_t dx = i < r ? r - i : i > w - 1 - r ? i - (w - 1 - r) : 0;
      int32_t dy = j < r ? r - j : j > h - 1 - r ? j - (h - 1 - r) : 0;
      if (dx * dx + dy * dy <= r * r + r) drawPixel(x + i, y + j, c);
    }
}

void TFT_eSPI::fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c) {
  int32_t minx = std::min({x0, x1, x2}), maxx = std::max({x0, x1, x2});
  int32_t miny = std::min({y0, y1, y2}), maxy = std::max({y0, y1, y2});
  auto edge = [](int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t px, int32_t py) {
    return (int64_t)(bx - ax) * (py - ay) - (int64_t)(by - ay) * (px - ax);
  };
  for (int32_t y = miny; y <= maxy; ++y)
    for (int32_t x = minx; x <= maxx; ++x) {
      int64_t a = edge(x0, y0, x1, y1, x, y), b = edge(x1, y1, x2, y2, x, y), d = edge(x2, y2, x0, y0, x, y);
      if ((a >= 0 && b >= 0 && d >= 0) || (a <= 0 && b <= 0 && d <= 0)) drawPixel(x, y, c);
    }
}

void TFT_eSPI::blend(int32_t x, int32_t y, float a, uint32_t c, uint32_t bg) {
  if (a <= 0.02f || x < vx || y < vy || x >= vw || y >= vh) return;
  if (a >= 0.98f) { fb[y * W + x] = c; return; }
  uint16_t b = bg == 0x00FFFFFF ? fb[y * W + x] : bg;
  auto ch = [&](int shift, int mask) {
    float f = ((c >> shift) & mask) * a + ((b >> shift) & mask) * (1 - a);
    return (uint16_t)((int)(f + 0.5f) & mask) << shift;
  };
  fb[y * W + x] = ch(11, 31) | ch(5, 63) | ch(0, 31);
}

void TFT_eSPI::fillSmoothCircle(int32_t x0, int32_t y0, int32_t r, uint32_t c, uint32_t bg) {
  for (int32_t y = -r - 1; y <= r + 1; ++y)
    for (int32_t x = -r - 1; x <= r + 1; ++x)
      blend(x0 + x, y0 + y, clamp01(r + 0.5f - std::sqrt(float(x * x + y * y))), c, bg);
}

void TFT_eSPI::fillSmoothRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t c, uint32_t bg) {
  float cx = x + (w - 1) / 2.0f, cy = y + (h - 1) / 2.0f, hx = (w - 1) / 2.0f - r, hy = (h - 1) / 2.0f - r;
  for (int32_t j = y - 1; j <= y + h; ++j)
    for (int32_t i = x - 1; i <= x + w; ++i) {
      float qx = std::max(std::fabs(i - cx) - hx, 0.0f), qy = std::max(std::fabs(j - cy) - hy, 0.0f);
      blend(i, j, clamp01(r + 0.5f - std::sqrt(qx * qx + qy * qy)), c, bg);
    }
}

void TFT_eSPI::drawWedgeLine(float ax, float ay, float bx, float by, float ar, float br, uint32_t c, uint32_t bg) {
  int32_t x0 = std::floor(std::min(ax - ar, bx - br)) - 1, x1 = std::ceil(std::max(ax + ar, bx + br)) + 1;
  int32_t y0 = std::floor(std::min(ay - ar, by - br)) - 1, y1 = std::ceil(std::max(ay + ar, by + br)) + 1;
  float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy + 1e-6f;
  for (int32_t y = y0; y <= y1; ++y)
    for (int32_t x = x0; x <= x1; ++x) {
      float t = clamp01(((x - ax) * dx + (y - ay) * dy) / len2);
      float px = ax + t * dx - x, py = ay + t * dy - y;
      blend(x, y, clamp01(ar + (br - ar) * t + 0.5f - std::sqrt(px * px + py * py)), c, bg);
    }
}

void TFT_eSPI::drawSmoothArc(int32_t x0, int32_t y0, int32_t r, int32_t ir, uint32_t, uint32_t, uint32_t c, uint32_t bg, bool) {
  float mid = (r + ir) / 2.0f, half = (r - ir) / 2.0f;
  for (int32_t y = -r - 1; y <= r + 1; ++y)
    for (int32_t x = -r - 1; x <= r + 1; ++x)
      blend(x0 + x, y0 + y, clamp01(half + 0.5f - std::fabs(std::sqrt(float(x * x + y * y)) - mid)), c, bg);
}

void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  for (int32_t j = 0; j < h; ++j)
    for (int32_t i = 0; i < w; ++i) {
      uint16_t v = data[j * w + i];
      drawPixel(x + i, y + j, swap ? v : uint16_t((v >> 8) | (v << 8)));
    }
}

int16_t TFT_eSPI::fontHeight(uint8_t font) { return fontInfo(font).height; }

int16_t TFT_eSPI::textWidth(const char* s, uint8_t font) {
  FontInfo f = fontInfo(font);
  if (!f.widths) return 0;
  int w = 0;
  for (; *s; ++s) if ((uint8_t)*s >= 32 && (uint8_t)*s <= 127) w += f.widths[(uint8_t)*s - 32];
  return w;
}

// Mirrors TFT_eSPI::drawChar for font 2 (bitmap) and the RLE fonts.
int16_t TFT_eSPI::drawChar(uint16_t ch, int32_t x, int32_t y, uint8_t font) {
  if (ch < 32 || ch > 127) return 0;
  FontInfo f = fontInfo(font);
  if (!f.chars) return 0;
  int width = f.widths[ch - 32], height = f.height;
  const uint8_t* p = f.chars[ch - 32];
  bool transparent = fg == bgc;
  if (font == 2) {
    int bytes = (width + 6) / 8;
    for (int i = 0; i < height; ++i) {
      if (!transparent) fillRect(x, y + i, width, 1, bgc);
      for (int k = 0; k < bytes; ++k) {
        uint8_t line = p[bytes * i + k];
        for (int b = 0; b < 8; ++b) if (line & (0x80 >> b)) drawPixel(x + k * 8 + b, y + i, fg);
      }
    }
    return width;
  }
  int total = width * height, pc = 0;
  while (pc < total) {
    uint8_t line = *p++;
    bool on = line & 0x80;
    int run = (line & 0x7f) + 1;
    for (int n = 0; n < run && pc < total; ++n, ++pc)
      if (on || !transparent) drawPixel(x + pc % width, y + pc / width, on ? fg : bgc);
  }
  return width;
}

int16_t TFT_eSPI::drawString(const char* s, int32_t x, int32_t y, uint8_t font) {
  uint16_t cw = textWidth(s, font), chh = fontHeight(font);
  uint8_t padding = 1, base = fontInfo(font).baseline;
  switch (datum) {
    case TC_DATUM: x -= cw / 2; padding += 1; break;
    case TR_DATUM: x -= cw; padding += 2; break;
    case ML_DATUM: y -= chh / 2; break;
    case MC_DATUM: x -= cw / 2; y -= chh / 2; padding += 1; break;
    case MR_DATUM: x -= cw; y -= chh / 2; padding += 2; break;
    case BL_DATUM: y -= chh; break;
    case BC_DATUM: x -= cw / 2; y -= chh; padding += 1; break;
    case BR_DATUM: x -= cw; y -= chh; padding += 2; break;
    case L_BASELINE: y -= base; break;
    case C_BASELINE: x -= cw / 2; y -= base; padding += 1; break;
    case R_BASELINE: x -= cw; y -= base; padding += 2; break;
  }
  int sum = 0;
  for (const char* c = s; *c; ++c) sum += drawChar((uint8_t)*c, x + sum, y, font);
  if (padX > cw && fg != bgc) {
    int padXc = x + cw;
    switch (padding) {
      case 1: fillRect(padXc, y, padX - cw, chh, bgc); break;
      case 2:
        fillRect(padXc, y, (padX - cw) >> 1, chh, bgc);
        fillRect(x - ((padX - cw) >> 1), y, (padX - cw) >> 1, chh, bgc);
        break;
      case 3:
        if (padXc > padX) padXc = padX;
        fillRect(x + cw - padXc, y, padXc - cw, chh, bgc);
        break;
    }
  }
  return sum;
}
