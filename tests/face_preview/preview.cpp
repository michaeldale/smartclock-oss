// Renders every clock face from src/faces.cpp into raw RGB565 frames, and checks that
// incremental redraws (second by second, across minute changes and a weather update)
// end up pixel-identical to a full redraw. run_face_preview.py builds and runs this.
#include <Arduino.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <filesystem>
#include <vector>
#include "config.h"
#include "faces.h"
#include "weather.h"

Config cfg;
Weather wx;
Forecast fc;
// Integration hooks used by faces.cpp (integrations.cpp is not built here).
static std::string facesDir;
static uint32_t facesVer = 1;
static std::vector<std::pair<std::string, std::string>> previewVars;
uint32_t customFacesVersion() { return facesVer; }
bool customFaceExists(const String& name) {
  FILE* f = fopen((facesDir + "/" + name.c_str() + ".json").c_str(), "rb");
  if (f) fclose(f);
  return f != nullptr;
}
String faceVar(const String& key) {
  for (auto& kv : previewVars) if (kv.first == key.c_str()) return String(kv.second);
  return String();
}
bool photoEnabled(const String&) { return true; }
LittleFSClass LittleFS;
static uint32_t fakeMillis = 0;
uint32_t millis() { return fakeMillis; }
void delay(unsigned long ms) { fakeMillis += ms; }
TFT_eSPI& clockDisplay() { static TFT_eSPI panel; return panel; }

static std::string outDir;
static void save(const std::string& name) {
  FILE* f = fopen((outDir + "/" + name + ".raw").c_str(), "wb");
  fwrite(clockDisplay().fb, 2, TFT_eSPI::W * TFT_eSPI::H, f);
  fclose(f);
}

static struct tm at(int h, int m, int s) {   // Monday 5 October 2026
  struct tm t{};
  t.tm_year = 126; t.tm_mon = 9; t.tm_mday = 5; t.tm_wday = 1; t.tm_yday = 277;
  t.tm_hour = h; t.tm_min = m; t.tm_sec = s;
  return t;
}
static void tick(struct tm& t) {
  if (++t.tm_sec == 60) { t.tm_sec = 0; if (++t.tm_min == 60) { t.tm_min = 0; ++t.tm_hour; } }
  fakeMillis += 1000;
}

static void setWeather(const char* icon, const char* main, const char* desc, float temp) {
  wx.valid = true; wx.temp = temp; wx.feels = temp - 1.6f; wx.tempMin = temp - 6; wx.tempMax = temp + 3;
  wx.humidity = 64; wx.wind = 4.2f; wx.main = main; wx.desc = desc; wx.icon = icon; wx.cityName = "Sydney";
  wx.version++;
}
static void setForecast(std::vector<const char*> icons) {
  fc.valid = true; fc.count = (int)icons.size(); fc.version++;
  const float hi[] = {24, 27, 19, 22}, lo[] = {15, 17, 12, 13};
  const int pop[] = {60, 0, 20, 80};
  for (int i = 0; i < fc.count; ++i) {
    fc.days[i].wday = (1 + i) % 7; fc.days[i].today = i == 0;
    fc.days[i].hi = hi[i]; fc.days[i].lo = lo[i]; fc.days[i].pop = pop[i]; fc.days[i].icon = icons[i];
  }
}

static void fresh(int face, const struct tm& t, const std::string& name) {
  faceInvalidate();
  faceDraw(face, t, true);
  save(name);
}

int main(int argc, char** argv) {
  outDir = argc > 1 ? argv[1] : ".";
  const char* names[] = {"classic", "weather", "photo", "dial", "simple", "forecast", "flip"};
  setWeather("03d", "Clouds", "scattered clouds", 21.4f);
  setForecast({"10d", "01d", "04d", "11d"});

  // 1. Every face at 9:54:26 pm.
  for (int f = 0; f < FACE_COUNT; ++f) fresh(f, at(21, 54, 26), std::string("face_") + std::to_string(f) + "_" + names[f]);

  // 2. Incremental vs full redraw.
  int failures = 0;
  for (int f = 0; f < FACE_COUNT; ++f) {
    if (f == FACE_PHOTO) continue;   // the photo advances on a timer by design
    struct tm t = at(21, 58, 50);
    faceInvalidate();
    faceDraw(f, t, true);
    for (int s = 0; s < 150; ++s) {
      tick(t);
      if (s == 40) setWeather("10d", "Rain", "light rain", 18.2f);
      if (s == 41) setForecast({"10d", "01d", "04d", "11d"});
      faceDraw(f, t, true);
    }
    std::vector<uint16_t> inc(clockDisplay().fb, clockDisplay().fb + TFT_eSPI::W * TFT_eSPI::H);
    save(std::string("incremental_") + names[f]);
    faceInvalidate();
    faceDraw(f, t, true);
    int diff = 0;
    for (int i = 0; i < TFT_eSPI::W * TFT_eSPI::H; ++i) diff += inc[i] != clockDisplay().fb[i];
    printf("incremental %-9s %5d pixels differ from a full redraw\n", names[f], diff);
    if (diff) { failures++; save(std::string("incremental_") + names[f] + "_expected"); }
    setWeather("03d", "Clouds", "scattered clouds", 21.4f);
  }

  // 3. Variants: other icons, units and formats.
  setWeather("01n", "Clear", "clear sky", 14.0f);
  fresh(FACE_WEATHER, at(21, 54, 26), "variant_weather_clear_night");
  setWeather("11d", "Thunderstorm", "thunderstorm with heavy rain", -3.0f);
  fresh(FACE_WEATHER, at(21, 54, 26), "variant_weather_storm_negative");
  setForecast({"02d", "09d", "13d", "50d"});
  fresh(FACE_FORECAST, at(21, 54, 26), "variant_forecast_icons");
  cfg.hour12 = false; cfg.celsius = false;
  setWeather("02d", "Clouds", "few clouds", 21.4f);
  fresh(FACE_CLASSIC, at(9, 5, 7), "variant_classic_24h_fahrenheit");
  fresh(FACE_FLIP, at(9, 5, 7), "variant_flip_24h");
  fresh(FACE_DIAL, at(3, 40, 15), "variant_dial_340");
  cfg.hour12 = true; cfg.celsius = true;

  // 4. Custom faces (integrations): the settings page's example, and one using
  // variables, a bar and transparent text; then the incremental check for both.
  facesDir = outDir + "/faces";
  std::filesystem::create_directories(facesDir);
#ifdef _WIN32
  _putenv_s("FACES_DIR", facesDir.c_str());
#else
  setenv("FACES_DIR", facesDir.c_str(), 1);
#endif
  auto writeFace = [&](const char* name, const char* json) {
    FILE* f = fopen((facesDir + "/" + name + ".json").c_str(), "wb");
    fputs(json, f); fclose(f); facesVer++;
  };
  writeFace("example", R"({"bg":"#000000","items":[
    {"type":"text","x":120,"y":24,"text":"{dow} {day} {mon3}","font":2,"color":"#9aa0a6","align":"mc","bg":"#000000","pad":200},
    {"type":"text","x":120,"y":78,"text":"{time}","font":6,"color":"#ffa826","align":"mc","bg":"#000000","pad":220},
    {"type":"ring","x":60,"y":170,"r":44,"w":8,"value":"{hum}","max":100,"color":"#6ab7ff"},
    {"type":"text","x":60,"y":170,"text":"{hum}%","font":4,"color":"#ffffff","align":"mc","bg":"#000000","pad":60},
    {"type":"icon","x":180,"y":155,"size":56,"icon":"{icon}","bg":"#000000"},
    {"type":"text","x":180,"y":205,"text":"{temp} {unit}","font":4,"color":"#4cd9a0","align":"mc","bg":"#000000","pad":90}]})");
  writeFace("power", R"({"bg":"#101820","items":[
    {"type":"rect","x":10,"y":10,"w":220,"h":220,"r":18,"color":"#1c2733"},
    {"type":"text","x":120,"y":40,"text":"Solar now","font":4,"color":"#9aa0a6","align":"mc"},
    {"type":"text","x":120,"y":100,"text":"{solar}","font":6,"color":"#ffd60a","align":"mc","bg":"#1c2733","pad":200},
    {"type":"text","x":120,"y":140,"text":"kW of {cap} kW","font":2,"color":"#9aa0a6","align":"mc","bg":"#1c2733","pad":200},
    {"type":"bar","x":30,"y":170,"w":180,"h":14,"value":"{pct}","max":100,"color":"#30d158","track":"#2c3a48"},
    {"type":"line","x":30,"y":200,"x2":210,"y2":200,"w":2,"color":"#2c3a48"},
    {"type":"text","x":120,"y":215,"text":"{time}  {hh}:{mm}:{ss}","font":2,"color":"#ffffff","align":"mc","bg":"#1c2733","pad":200},
    {"type":"circle","x":36,"y":40,"r":6,"color":"#30d158"}]})");
  cfg.facesApi = true;
  previewVars = {{"solar", "3.4"}, {"cap", "6.6"}, {"pct", "52"}};
  setWeather("03d", "Clouds", "scattered clouds", 21.4f);
  for (const char* name : {"example", "power"}) {
    cfg.customFace = name;
    fresh(FACE_CUSTOM, at(21, 54, 26), std::string("custom_") + name);
    struct tm t = at(21, 58, 50);
    faceInvalidate(); faceDraw(FACE_CUSTOM, t, true);
    for (int s = 0; s < 150; ++s) {
      tick(t);
      if (s == 30) previewVars = {{"solar", "4.1"}, {"cap", "6.6"}, {"pct", "62"}};
      if (s == 40) setWeather("10d", "Rain", "light rain", 18.2f);
      faceDraw(FACE_CUSTOM, t, true);
    }
    std::vector<uint16_t> inc(clockDisplay().fb, clockDisplay().fb + TFT_eSPI::W * TFT_eSPI::H);
    faceInvalidate(); faceDraw(FACE_CUSTOM, t, true);
    int diff = 0;
    for (int i = 0; i < TFT_eSPI::W * TFT_eSPI::H; ++i) diff += inc[i] != clockDisplay().fb[i];
    printf("incremental custom:%-7s %5d pixels differ from a full redraw\n", name, diff);
    if (diff) failures++;
    setWeather("03d", "Clouds", "scattered clouds", 21.4f);
  }
  cfg.customFace = "missing";
  fresh(FACE_CUSTOM, at(21, 54, 26), "custom_missing");
  cfg.facesApi = false;

  // 5. No data yet.
  wx = Weather(); fc = Forecast();
  fresh(FACE_CLASSIC, at(21, 54, 26), "empty_classic");
  fresh(FACE_WEATHER, at(21, 54, 26), "empty_weather_waiting");
  wx.error = "City not found";
  fresh(FACE_FORECAST, at(21, 54, 26), "empty_forecast_bad_city");
  wx.error = "";
  #ifdef _WIN32
  _putenv_s("PHOTO_DIR", "");
#else
  unsetenv("PHOTO_DIR");
#endif
  facePhotosChanged();
  fresh(FACE_PHOTO, at(21, 54, 26), "empty_photo");
  faceInvalidate(); faceDraw(FACE_CLASSIC, at(0, 0, 0), false); save("empty_setting_time");

  printf(failures ? "FAIL: %d faces leave pixels behind\n" : "PASS: incremental redraws match full redraws\n", failures);
  return failures ? 1 : 0;
}
