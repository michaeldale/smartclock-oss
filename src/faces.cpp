// Clock faces for the 240x240 ST7789. There is no RAM for a full-screen buffer, so
// every face redraws only the regions whose content changed, and text is drawn with
// a background colour (or padding) so it overwrites what was there.
#include "faces.h"
#include <Arduino.h>
#include <algorithm>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <tjpgd.h>   // the decoder core only: the TJpg_Decoder wrapper links SPIFFS (~32 KB)
#include "config.h"
#include "weather.h"

TFT_eSPI& clockDisplay();   // main.cpp
static TFT_eSPI& D() { return clockDisplay(); }

namespace {

// ---- Palette (RGB565). Accents come from the user's color1..3.
constexpr uint16_t BG = 0x0000, PANEL = 0x18E3, PANEL_HI = 0x2945, DIM = 0x528A,
                   MUTED = 0x9D14, WHITE = 0xFFFF, CLOUD = 0xE75D, CLOUD_DARK = 0xA536,
                   RAIN = 0x5D9F, SUN = 0xFD84, BOLT = 0xFEA1, MOON = 0xF738;

const char* const DAY_LONG[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* const DAY_SHORT[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const MONTH_LONG[] = {"January", "February", "March", "April", "May", "June", "July",
                                  "August", "September", "October", "November", "December"};
const char* const MONTH_SHORT[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

int drawnFace = -1;      // -2 = "setting the time" screen
bool dirty = true;
int photosCached = -1;   // -1 = recount

// ---- Text and number helpers --------------------------------------------------
void text(const char* s, int x, int y, uint8_t font, uint16_t fg, uint16_t bg, uint8_t datum, int pad = 0) {
  TFT_eSPI& d = D();
  d.setTextColor(fg, bg);
  d.setTextDatum(datum);
  d.setTextPadding(pad);
  d.drawString(s, x, y, font);
  d.setTextPadding(0);
}

void textClear(const char* s, int x, int y, uint8_t font, uint16_t fg, uint8_t datum) {   // transparent
  TFT_eSPI& d = D();
  d.setTextColor(fg);
  d.setTextDatum(datum);
  d.drawString(s, x, y, font);
}

int displayTemp(float c) { return lroundf(cfg.celsius ? c : c * 9 / 5 + 32); }

int clockHour(const struct tm& t) {
  int h = t.tm_hour;
  if (cfg.hour12) { h %= 12; if (!h) h = 12; }
  return h;
}

void ring(int x, int y, int r, uint16_t fg, uint16_t bg) {   // the degree sign; no font has one
  D().fillSmoothCircle(x, y, r, fg, bg);
  D().fillCircle(x, y, r > 3 ? r - 2 : r - 1, bg);
}

// A temperature with a drawn degree ring, centred on (cx, cy).
void temp(int cx, int cy, int value, uint8_t font, uint16_t fg, uint16_t bg) {
  char buf[8]; snprintf(buf, sizeof(buf), "%d", value);
  int r = font >= 6 ? 6 : font == 4 ? 4 : 3;
  int w = D().textWidth(buf, font), total = w + 2 * r + 3;
  int x0 = cx - total / 2, top = cy - D().fontHeight(font) / 2;
  text(buf, x0, cy, font, fg, bg, ML_DATUM);
  ring(x0 + w + r + 2, top + r + (font >= 6 ? 4 : 2), r, fg, bg);
}

void colon(int cx, int cy, uint16_t c, uint16_t bg) {
  D().fillSmoothCircle(cx, cy - 15, 5, c, bg);
  D().fillSmoothCircle(cx, cy + 15, 5, c, bg);
}

// HH:MM in font 8 (55 px digits). "12:34" is 249 px wide in that font, so the colon
// is drawn as two dots to fit the 240 px panel. The group is centred, so "9:41"
// sits in the middle too; the band is cleared only when the width changes.
int bigTimeColonX(int cx, const struct tm& t) {
  char h[4]; snprintf(h, sizeof(h), cfg.hour12 ? "%d" : "%02d", clockHour(t));
  return cx + (D().textWidth(h, 8) - 110) / 2;   // 110 = two digits
}

void bigTime(int cx, int cy, const struct tm& t, uint16_t fg, uint16_t bg) {
  static int lastColon = -1;
  char h[4], m[4];
  snprintf(h, sizeof(h), cfg.hour12 ? "%d" : "%02d", clockHour(t));
  snprintf(m, sizeof(m), "%02d", t.tm_min);
  int c = bigTimeColonX(cx, t);
  if (c != lastColon) { D().fillRect(0, cy - 38, 240, 76, bg); lastColon = c; }
  text(h, c - 10, cy, 8, fg, bg, MR_DATUM);
  text(m, c + 10, cy, 8, fg, bg, ML_DATUM);
  colon(c, cy, fg, bg);
}

void smallTime(char* out, size_t n, const struct tm& t) {
  snprintf(out, n, cfg.hour12 ? "%d:%02d" : "%02d:%02d", clockHour(t), t.tm_min);
}

const char* ampm(const struct tm& t) { return cfg.hour12 ? (t.tm_hour < 12 ? "AM" : "PM") : ""; }

// ---- Weather icons, drawn from primitives at any size ----------------------------
// Anti-aliased shapes blend their edges into a single background colour, so where
// circles overlap the seams are painted over with a plain fill afterwards.
void cloud(int cx, int cy, int s, uint16_t c, uint16_t bg) {
  struct { float x, y, r; } puffs[] = {{-0.22f, 0.04f, 0.22f}, {0.02f, -0.08f, 0.30f}, {0.26f, 0.06f, 0.20f}};
  int bx = cx - s * 0.44f, by = cy + s * 0.02f, bw = s * 0.88f, bh = s * 0.24f;
  for (auto& p : puffs) D().fillSmoothCircle(cx + p.x * s, cy + p.y * s, p.r * s, c, bg);
  D().fillSmoothRoundRect(bx, by, bw, bh, bh / 2, c, bg);
  for (auto& p : puffs) D().fillCircle(cx + p.x * s, cy + p.y * s, p.r * s - 1, c);
  D().fillRoundRect(bx + 1, by + 1, bw - 2, bh - 2, bh / 2 - 1, c);
}

void sun(int cx, int cy, int R, uint16_t bg) {
  D().fillSmoothCircle(cx, cy, R * 0.55f, SUN, bg);
  float w = R > 30 ? 2.2f : R > 14 ? 1.5f : 1.0f;
  for (int i = 0; i < 8; ++i) {
    float a = i * PI / 4;
    D().drawWedgeLine(cx + sinf(a) * R * 0.72f, cy - cosf(a) * R * 0.72f,
                      cx + sinf(a) * R, cy - cosf(a) * R, w, w, SUN, bg);
  }
}

void moon(int cx, int cy, int r, uint16_t bg) {
  D().fillSmoothCircle(cx, cy, r, MOON, bg);
  D().fillCircle(cx + r * 0.45f, cy - r * 0.35f, r * 0.8f, bg);   // the cut-out
}

void weatherIcon(int cx, int cy, int s, const String& icon, uint16_t bg) {
  int code = icon.length() >= 2 ? icon.substring(0, 2).toInt() : 0;
  bool night = icon.endsWith("n");
  auto drops = [&](uint16_t c, bool snow) {
    float pos[][2] = {{-0.22f, 0.20f}, {0.0f, 0.26f}, {0.22f, 0.20f}};
    for (auto& p : pos) {
      int x = cx + p[0] * s, y = cy + p[1] * s;
      if (snow) D().fillSmoothCircle(x, y + s * 0.06f, std::max(2, s / 22), c, bg);
      else D().drawWedgeLine(x, y, x - s * 0.06f, y + s * 0.16f, std::max(1.0f, s / 44.0f), std::max(1.0f, s / 44.0f), c, bg);
    }
  };
  switch (code) {
    case 1: if (night) moon(cx, cy, s * 0.30f, bg); else sun(cx, cy, s * 0.42f, bg); break;
    case 2:
      if (night) moon(cx - s * 0.14f, cy - s * 0.14f, s * 0.22f, bg); else sun(cx - s * 0.14f, cy - s * 0.16f, s * 0.30f, bg);
      cloud(cx + s * 0.08f, cy + s * 0.06f, s * 0.78f, CLOUD, bg);
      break;
    case 3: cloud(cx, cy, s * 0.9f, CLOUD, bg); break;
    case 4:
      cloud(cx + s * 0.14f, cy - s * 0.12f, s * 0.68f, CLOUD_DARK, bg);
      cloud(cx - s * 0.06f, cy + s * 0.06f, s * 0.80f, CLOUD, bg);
      break;
    case 9: case 10:
      if (code == 10 && !night) sun(cx + s * 0.18f, cy - s * 0.24f, s * 0.24f, bg);
      cloud(cx, cy - s * 0.12f, s * 0.86f, CLOUD, bg);
      drops(RAIN, false);
      break;
    case 11: {
      cloud(cx, cy - s * 0.12f, s * 0.86f, CLOUD_DARK, bg);
      D().fillTriangle(cx + s * 0.06f, cy + s * 0.08f, cx - s * 0.12f, cy + s * 0.30f, cx + s * 0.02f, cy + s * 0.30f, BOLT);
      D().fillTriangle(cx + s * 0.08f, cy + s * 0.24f, cx - s * 0.06f, cy + s * 0.46f, cx - s * 0.04f, cy + s * 0.24f, BOLT);
      break;
    }
    case 13: cloud(cx, cy - s * 0.12f, s * 0.86f, CLOUD, bg); drops(WHITE, true); break;
    case 50:
      for (int i = 0; i < 3; ++i) {
        int w = s * (i == 1 ? 0.86f : 0.66f), h = std::max(3, s / 12);
        D().fillSmoothRoundRect(cx - w / 2 + (i == 2 ? s * 0.08f : 0), cy + (i - 1) * s * 0.2f - h / 2, w, h, h / 2, CLOUD_DARK, bg);
      }
      break;
    default: cloud(cx, cy, s * 0.9f, CLOUD_DARK, bg); break;
  }
}

String dayIcon(const String& icon) {   // forecast days always use the day variant
  return icon.length() >= 2 ? icon.substring(0, 2) + "d" : icon;
}

void placeholder(int iconY, const char* title, const char* line1, const char* line2) {
  text(title, 120, iconY + 62, 4, WHITE, BG, MC_DATUM);
  text(line1, 120, iconY + 90, 2, MUTED, BG, MC_DATUM);
  if (line2) text(line2, 120, iconY + 108, 2, MUTED, BG, MC_DATUM);
}

// What to say while there is no data: the last error, or waiting.
void weatherPlaceholder(const char* title) {
  if (wx.error == "City not found" || wx.error == "Set a city") {
    placeholder(92, title, wx.error.c_str(), "Check the city in settings");
    return;
  }
  placeholder(92, title, wx.error.length() ? wx.error.c_str() : "Waiting for the first update", nullptr);
}

// ---- Setting the time -----------------------------------------------------------
void syncScreen() {
  D().fillScreen(BG);
  D().fillSmoothCircle(120, 88, 34, PANEL_HI, BG);
  D().drawWedgeLine(120, 88, 120, 66, 3, 2, WHITE, PANEL_HI);
  D().drawWedgeLine(120, 88, 136, 98, 3, 2, cfg.color1, PANEL_HI);
  D().fillSmoothCircle(120, 88, 4, WHITE, PANEL_HI);
  text("Setting the time", 120, 150, 4, WHITE, BG, MC_DATUM);
  text("Waiting for a time server", 120, 178, 2, MUTED, BG, MC_DATUM);
}

// ---- 0 Classic: date, big time, seconds bar, weather line ------------------------
void classic(const struct tm& t, bool force) {
  static int day = -1, min = -1, sec = -1;
  static uint32_t wxv = ~0u;
  if (force) { day = min = sec = -1; wxv = ~0u; }
  const int barX = 30, barY = 166, barW = cfg.hour12 ? 150 : 180, barH = 4;
  if (t.tm_yday != day) {
    char date[24];
    snprintf(date, sizeof(date), "%d %s", t.tm_mday, MONTH_LONG[t.tm_mon]);
    String wd = DAY_LONG[t.tm_wday]; wd.toUpperCase();
    text(wd.c_str(), 120, 28, 2, cfg.color2, BG, MC_DATUM, 200);
    text(date, 120, 52, 4, WHITE, BG, MC_DATUM, 230);
    day = t.tm_yday;
  }
  if (t.tm_min != min) {
    bigTime(120, 112, t, cfg.color1, BG);
    D().fillRect(0, barY - 8, 240, 20, BG);
    D().fillRoundRect(barX, barY, barW, barH, 2, PANEL_HI);
    if (cfg.hour12) text(ampm(t), 210, barY + 2, 2, MUTED, BG, MC_DATUM);
    min = t.tm_min; sec = -1;
  }
  if (t.tm_sec != sec) {
    D().fillRoundRect(barX, barY, std::max(barH, barW * (t.tm_sec + 1) / 60), barH, 2, cfg.color2);
    sec = t.tm_sec;
  }
  if (wx.version != wxv) {
    D().fillRect(0, 184, 240, 56, BG);
    if (wx.valid) {
      char tb[8]; snprintf(tb, sizeof(tb), "%d", displayTemp(wx.temp));
      int tw = D().textWidth(tb, 4) + 12, mw = D().textWidth(wx.main, 2);
      int x = 120 - (36 + 8 + tw + 8 + mw) / 2;
      weatherIcon(x + 18, 206, 36, wx.icon, BG);
      temp(x + 44 + tw / 2, 207, displayTemp(wx.temp), 4, cfg.color3, BG);
      text(wx.main.c_str(), x + 44 + tw + 8, 208, 2, MUTED, BG, ML_DATUM);
    }
    wxv = wx.version;
  }
}

// ---- 3 Dial: anti-aliased hands; only the hands are erased and redrawn ------------
void dialHand(float deg, float len, float tail, float r0, float r1, uint16_t c) {
  float a = deg * DEG_TO_RAD, sx = sinf(a), cy = cosf(a);
  D().drawWedgeLine(120 - sx * tail, 120 + cy * tail, 120 + sx * len, 120 - cy * len, r0, r1, c, BG);
}

void dial(const struct tm& t, bool force) {
  static int sec = -1;
  static float pH = -1, pM = 0, pS = 0;
  if (force) {
    D().drawSmoothArc(120, 120, 119, 116, 0, 360, PANEL_HI, BG);
    for (int i = 0; i < 60; ++i) {
      float a = i * 6 * DEG_TO_RAD, sx = sinf(a), cx = cosf(a);
      if (i % 5) { D().fillSmoothCircle(120 + sx * 108, 120 - cx * 108, 1, DIM, BG); continue; }
      bool major = i % 15 == 0;
      D().drawWedgeLine(120 + sx * (major ? 98 : 102), 120 - cx * (major ? 98 : 102),
                        120 + sx * 111, 120 - cx * 111, major ? 2.5f : 1.5f, major ? 2.5f : 1.5f,
                        major ? WHITE : MUTED, BG);
    }
    sec = -1; pH = -1;
  }
  if (t.tm_sec == sec) return;
  sec = t.tm_sec;
  float aS = t.tm_sec * 6.0f, aM = t.tm_min * 6.0f + t.tm_sec * 0.1f, aH = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
  if (pH >= 0) {   // erase the previous hands with the same geometry, slightly wider
    dialHand(pS, 94, 18, 1.6f, 1.1f, BG);
    dialHand(pM, 84, 0, 3.6f, 2.1f, BG);
    dialHand(pH, 56, 0, 4.6f, 3.1f, BG);
  }
  // Complications are redrawn every second because the hands sweep across them.
  char date[12]; snprintf(date, sizeof(date), "%s %d", DAY_SHORT[t.tm_wday], t.tm_mday);
  text(date, 120, 170, 2, MUTED, BG, MC_DATUM, 70);
  if (wx.valid) temp(120, 72, displayTemp(wx.temp), 4, cfg.color3, BG);
  dialHand(aH, 56, 0, 4.0f, 2.5f, WHITE);
  dialHand(aM, 84, 0, 3.0f, 1.5f, WHITE);
  dialHand(aS, 94, 18, 1.0f, 0.6f, cfg.color1);
  D().fillSmoothCircle(120, 120, 6, cfg.color1, BG);
  D().fillSmoothCircle(120, 120, 2, BG, cfg.color1);
  pH = aH; pM = aM; pS = aS;
}

// ---- 1 Weather: big icon and temperature, details below -------------------------
void weather(const struct tm& t, bool force) {
  static int min = -1;
  static uint32_t wxv = ~0u;
  if (force) { min = -1; wxv = ~0u; }
  if (t.tm_min != min) {
    char tb[8]; smallTime(tb, sizeof(tb), t);
    char db[16]; snprintf(db, sizeof(db), "%s %d %s", DAY_SHORT[t.tm_wday], t.tm_mday, MONTH_SHORT[t.tm_mon]);
    text(tb, 16, 24, 4, WHITE, BG, ML_DATUM, 90);
    text(db, 224, 24, 2, MUTED, BG, MR_DATUM, 110);
    min = t.tm_min;
  }
  if (wx.version == wxv) return;
  wxv = wx.version;
  D().fillRect(0, 44, 240, 196, BG);
  if (!wx.valid) {
    cloud(120, 92, 84, DIM, BG);
    weatherPlaceholder("No weather yet");
    return;
  }
  weatherIcon(64, 104, 100, wx.icon, BG);
  char tb[8]; snprintf(tb, sizeof(tb), "%d", displayTemp(wx.temp));
  int w = D().textWidth(tb, 6);
  text(tb, 126, 96, 6, WHITE, BG, ML_DATUM);
  ring(126 + w + 8, 80, 6, WHITE, BG);
  text(cfg.celsius ? "C" : "F", 126 + w + 18, 80, 2, MUTED, BG, ML_DATUM);
  char hl[24]; snprintf(hl, sizeof(hl), "H %d   L %d", displayTemp(wx.tempMax), displayTemp(wx.tempMin));
  text(hl, 128, 132, 2, MUTED, BG, ML_DATUM);
  String desc = wx.desc; if (desc.length()) desc.setCharAt(0, toupper(desc[0]));
  text(desc.c_str(), 120, 170, D().textWidth(desc, 4) <= 228 ? 4 : 2, cfg.color2, BG, MC_DATUM);
  const char* labels[] = {"FEELS", "HUMIDITY", "WIND"};
  for (int i = 0; i < 3; ++i) text(labels[i], 42 + i * 78, 198, 2, MUTED, BG, MC_DATUM);
  temp(42, 222, displayTemp(wx.feels), 4, WHITE, BG);
  char hu[8]; snprintf(hu, sizeof(hu), "%d%%", wx.humidity);
  text(hu, 120, 222, 4, WHITE, BG, MC_DATUM);
  char wd[8]; snprintf(wd, sizeof(wd), "%d", (int)lroundf(cfg.mile ? wx.wind * 2.237f : wx.wind * 3.6f));
  int ww = D().textWidth(wd, 4), uw = D().textWidth(cfg.mile ? "mph" : "km/h", 2);
  int x0 = 198 - (ww + 3 + uw) / 2;
  text(wd, x0, 222, 4, WHITE, BG, ML_DATUM);
  text(cfg.mile ? "mph" : "km/h", x0 + ww + 3, 226, 2, MUTED, BG, ML_DATUM);
}

// ---- 5 Forecast: four day cards ---------------------------------------------------
void forecast(const struct tm& t, bool force) {
  static int min = -1;
  static uint32_t fcv = ~0u;
  if (force) { min = -1; fcv = ~0u; }
  if (t.tm_min != min) {
    String city = wx.cityName.length() ? wx.cityName : cfg.city;
    while (city.length() > 3 && D().textWidth(city, 4) > 150) city.remove(city.length() - 1);
    char tb[8]; smallTime(tb, sizeof(tb), t);
    text(city.c_str(), 12, 24, 4, WHITE, BG, ML_DATUM, 150);
    text(tb, 228, 24, 4, MUTED, BG, MR_DATUM, 70);
    min = t.tm_min;
  }
  if (fc.version == fcv && !force) return;
  fcv = fc.version;
  D().fillRect(0, 44, 240, 196, BG);
  if (!fc.valid) {
    cloud(120, 92, 84, DIM, BG);
    weatherPlaceholder("No forecast yet");
    return;
  }
  for (int i = 0; i < FORECAST_DAYS; ++i) {
    int x = 6 + i * 58, cx = x + 27;
    D().fillSmoothRoundRect(x, 50, 54, 182, 12, PANEL, BG);
    if (i >= fc.count) continue;
    const ForecastDay& d = fc.days[i];
    text(d.today ? "TODAY" : DAY_SHORT[d.wday], cx, 70, 2, d.today ? cfg.color2 : MUTED, PANEL, MC_DATUM);
    weatherIcon(cx, 110, 44, dayIcon(d.icon), PANEL);
    temp(cx, 152, displayTemp(d.hi), 4, WHITE, PANEL);
    temp(cx, 178, displayTemp(d.lo), 2, MUTED, PANEL);
    if (d.pop >= 10) {
      char p[12]; snprintf(p, sizeof(p), "%d%%", std::min(d.pop, 100));
      D().fillSmoothCircle(cx - 14, 210, 3, RAIN, PANEL);
      D().fillTriangle(cx - 17, 209, cx - 11, 209, cx - 14, 203, RAIN);
      text(p, cx - 8, 208, 2, RAIN, PANEL, ML_DATUM);
    }
  }
}

// ---- 6 Flip: split-flap cards with a fold animation ---------------------------------
constexpr int FLIP_Y = 34, FLIP_H = 136, FLIP_W = 114, FLIP_MID = FLIP_Y + FLIP_H / 2;

void flipCard(int x, const char* digits, const char* tag) {
  D().fillSmoothRoundRect(x, FLIP_Y, FLIP_W, FLIP_H, 12, PANEL, BG);
  D().fillSmoothRoundRect(x, FLIP_Y, FLIP_W, FLIP_H / 2 + 12, 12, PANEL_HI, BG);
  D().fillRect(x, FLIP_MID, FLIP_W, 13, PANEL);
  textClear(digits, x + FLIP_W / 2, FLIP_MID + 3, 8, WHITE, MC_DATUM);
  if (tag && *tag) textClear(tag, x + 12, FLIP_Y + 10, 2, MUTED, TL_DATUM);
  D().fillRect(x, FLIP_MID - 1, FLIP_W, 3, BG);   // the hinge
  D().fillCircle(x, FLIP_MID, 4, BG);
  D().fillCircle(x + FLIP_W - 1, FLIP_MID, 4, BG);
}

// The top flap falls: the new top half is revealed from the top edge down, then the
// flap's back shows the new bottom half growing from the hinge.
void flipAnimate(int x, const char* digits, const char* tag) {
  const int steps = 6, half = FLIP_H / 2;
  for (int k = 1; k <= steps; ++k) {
    D().setViewport(x, FLIP_Y, FLIP_W, half * k / steps, false);
    flipCard(x, digits, tag);
    D().resetViewport();
    delay(10);
  }
  for (int k = 1; k <= steps; ++k) {
    D().setViewport(x, FLIP_MID, FLIP_W, (half + 1) * k / steps, false);
    flipCard(x, digits, tag);
    D().resetViewport();
    delay(10);
  }
}

void flip(const struct tm& t, bool force) {
  static char ph[4] = "", pm[4] = "";
  static int day = -1;
  static uint32_t wxv = ~0u;
  if (force) { ph[0] = pm[0] = 0; day = -1; wxv = ~0u; }
  char h[4], m[4];
  snprintf(h, sizeof(h), cfg.hour12 ? "%d" : "%02d", clockHour(t));
  snprintf(m, sizeof(m), "%02d", t.tm_min);
  const char* tag = ampm(t);
  if (strcmp(h, ph)) { if (force || !ph[0]) flipCard(4, h, tag); else flipAnimate(4, h, tag); strcpy(ph, h); }
  if (strcmp(m, pm)) { if (force || !pm[0]) flipCard(122, m, nullptr); else flipAnimate(122, m, nullptr); strcpy(pm, m); }
  if (t.tm_yday != day) {
    char db[28];
    snprintf(db, sizeof(db), "%s %d %s", DAY_LONG[t.tm_wday], t.tm_mday, MONTH_LONG[t.tm_mon]);
    if (D().textWidth(db, 4) > 228) snprintf(db, sizeof(db), "%.3s %d %s", DAY_LONG[t.tm_wday], t.tm_mday, MONTH_SHORT[t.tm_mon]);
    text(db, 120, 194, 4, cfg.color2, BG, MC_DATUM, 236);
    day = t.tm_yday;
  }
  if (wx.version != wxv) {
    D().fillRect(0, 210, 240, 30, BG);
    if (wx.valid) {
      weatherIcon(96, 224, 24, wx.icon, BG);
      temp(130, 225, displayTemp(wx.temp), 2, MUTED, BG);
    }
    wxv = wx.version;
  }
}

// ---- 4 Simple: the time and the date, nothing else -------------------------------
void simple(const struct tm& t, bool force) {
  static int min = -1, sec = -1, day = -1;
  if (force) { min = sec = day = -1; }
  if (t.tm_min != min) { bigTime(120, 104, t, WHITE, BG); min = t.tm_min; }
  if (t.tm_sec != sec) { colon(bigTimeColonX(120, t), 104, t.tm_sec % 2 ? DIM : WHITE, BG); sec = t.tm_sec; }
  if (t.tm_yday != day) {
    char db[32];
    snprintf(db, sizeof(db), "%s  %d %s", DAY_LONG[t.tm_wday], t.tm_mday, MONTH_LONG[t.tm_mon]);
    String s = db; s.toUpperCase();
    text(s.c_str(), 120, 168, 2, MUTED, BG, MC_DATUM, 236);
    text(ampm(t), 120, 190, 2, cfg.color1, BG, MC_DATUM, 40);
    day = t.tm_yday;
  }
}

// ---- 2 Photo: JPEGs from /photo with the time on top -----------------------------
struct JpgIo { File* file; int ox, oy; };

size_t jpgIn(JDEC* jd, uint8_t* buf, size_t n) {
  File* f = static_cast<JpgIo*>(jd->device)->file;
  if (buf) return f->read(buf, n);
  return f->seek(n, SeekCur) ? n : 0;
}

int jpgOut(JDEC* jd, void* bmp, JRECT* r) {
  JpgIo* io = static_cast<JpgIo*>(jd->device);
  int y = io->oy + r->top;
  if (y >= 240) return 0;
  D().pushImage(io->ox + r->left, y, r->right - r->left + 1, r->bottom - r->top + 1, static_cast<uint16_t*>(bmp));
  return 1;
}

// Decodes a baseline JPEG straight to the panel, scaled by 1/2..1/8 to fit 240x240.
// The 3.6 KB workspace is only allocated while decoding.
bool drawJpeg(const String& path) {
  constexpr size_t POOL = 3600;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  void* pool = malloc(POOL);
  bool ok = false;
  JDEC jd{};
  jd.swap = 0;   // this tjpgd keeps a byte-swap flag across jd_prepare(); emit native RGB565
  JpgIo io{&f, 0, 0};
  if (pool && jd_prepare(&jd, jpgIn, pool, POOL, &io) == JDR_OK) {
    uint8_t scale = 0;
    while (((jd.width >> scale) > 240 || (jd.height >> scale) > 240) && scale < 3) ++scale;
    io.ox = (240 - (jd.width >> scale)) / 2;
    io.oy = (240 - (jd.height >> scale)) / 2;
    D().fillScreen(BG);
    D().setSwapBytes(true);   // tjpgd emits native-endian RGB565
    ok = jd_decomp(&jd, jpgOut, scale) == JDR_OK;
    D().setSwapBytes(false);
  }
  free(pool);
  f.close();
  return ok;
}

bool isPhoto(const String& name) {
  String n = name; n.toLowerCase();
  return n.endsWith(".jpg") || n.endsWith(".jpeg");
}

String photoAt(int index) {   // index modulo the number of photos
  Dir dir = LittleFS.openDir("/photo");
  int n = 0;
  String first;
  while (dir.next()) {
    if (!dir.isFile() || !isPhoto(dir.fileName())) continue;
    if (n == 0) first = dir.fileName();
    if (n++ == index) return "/photo/" + dir.fileName();
  }
  return first.length() ? "/photo/" + first : String();
}

void photo(const struct tm& t, bool force) {
  static int min = -1, index = -1;
  static uint32_t shown = 0;
  static bool have = false;
  if (force || millis() - shown > 60000) {
    shown = millis();
    min = -1;
    int count = photoCount();
    have = count > 0;
    if (!have) {
      if (!force) return;
      D().fillScreen(BG);
      D().fillSmoothRoundRect(84, 62, 72, 58, 10, PANEL_HI, BG);
      D().fillSmoothCircle(104, 82, 7, MUTED, PANEL_HI);
      D().fillTriangle(92, 112, 116, 88, 140, 112, DIM);
      placeholder(92, "No photos yet", "Upload photos in the", "clock's settings");
      return;
    }
    index = (index + 1) % count;
    if (!drawJpeg(photoAt(index))) {
      D().fillScreen(BG);
      placeholder(92, "Photo unreadable", "Use a baseline JPEG", nullptr);
    }
  }
  if (have && t.tm_min != min) {
    char tb[8]; smallTime(tb, sizeof(tb), t);
    D().fillSmoothRoundRect(66, 196, 108, 34, 17, BG, BG);
    text(tb, 120, 214, 4, WHITE, BG, MC_DATUM);
    min = t.tm_min;
  }
}

}  // namespace

// ---- Public -----------------------------------------------------------------------
void faceInvalidate() { dirty = true; }
void facePhotosChanged() { photosCached = -1; dirty = true; }

int photoCount() {
  if (photosCached < 0) {
    photosCached = 0;
    Dir dir = LittleFS.openDir("/photo");
    while (dir.next()) if (dir.isFile() && isPhoto(dir.fileName())) photosCached++;
  }
  return photosCached;
}

bool faceAvailable(int face) {
  switch (face) {
    case FACE_WEATHER:  return wx.valid;
    case FACE_FORECAST: return fc.valid;
    case FACE_PHOTO:    return photoCount() > 0;
    default:            return face >= 0 && face < FACE_COUNT;
  }
}

void faceDraw(int face, const struct tm& t, bool timeValid) {
  if (!timeValid) {
    if (drawnFace != -2 || dirty) syncScreen();
    drawnFace = -2; dirty = false;
    return;
  }
  bool force = dirty || face != drawnFace;
  if (force && face != FACE_PHOTO) D().fillScreen(BG);
  switch (face) {
    case FACE_WEATHER:  weather(t, force);  break;
    case FACE_PHOTO:    photo(t, force);    break;
    case FACE_DIAL:     dial(t, force);     break;
    case FACE_SIMPLE:   simple(t, force);   break;
    case FACE_FORECAST: forecast(t, force); break;
    case FACE_FLIP:     flip(t, force);     break;
    default:            classic(t, force);  break;
  }
  drawnFace = face; dirty = false;
}
