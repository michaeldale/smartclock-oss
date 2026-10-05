#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Persistent configuration. Mirrors the stock /config JSON keys so the existing
// web UI (and expectations) line up. Stored at /config.json in LittleFS.
struct Config {
  String ssid;
  String password;
  String city        = "Sydney";
  String ntp         = "pool.ntp.org";
  // Open-Meteo works from coordinates. The city is geocoded once and the result
  // cached here; geoFor records which city string it belongs to.
  float  lat = NAN, lon = NAN;
  String geoName, geoFor;
  int    timezoneMin = 600;          // minutes offset (stock stores minutes)
  // POSIX TZ rule, so daylight saving switches automatically. Takes precedence over
  // timezoneMin; empty means "fixed timezoneMin offset" (stock behaviour).
  String tz          = "AEST-10AEDT,M10.1.0,M4.1.0/3";   // Sydney/Melbourne/Canberra
  bool   celsius     = true;
  bool   hour12      = true;
  bool   mile        = false;
  int    theme       = 0;            // 0=Classic 1=Weather 2=Photo 3=Dial 4=Simple 5=Forecast 6=Flip
  int    themeInterval = 10;         // seconds; 0 = no rotation
  int    faceMask    = 0x7f;         // bit n set = face n takes part in rotation
  int    brightness  = 60;           // 0..100
  bool   nightMode   = true;
  int    nightBrightness = 15;       // 0..100
  int    startHour   = 23;           // night start
  int    stopHour    = 7;            // night end
  uint16_t color1 = 0xFD44;          // RGB565 accents: time and hands (amber #FFA826)
  uint16_t color2 = 0x6DBF;          // date and highlights (soft blue #6AB7FF)
  uint16_t color3 = 0x4ED4;          // weather (mint #4CD9A0)

  bool load() {
    if (!LittleFS.exists("/config.json")) return false;
    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();
    ssid        = doc["ssid"]        | ssid;
    password    = doc["password"]    | password;
    city        = doc["city"]        | city;
    ntp         = doc["ntp"]         | ntp;
    if (doc["lat5"].is<long>() && doc["lon5"].is<long>()) {   // 1e-5 degrees
      lat = (doc["lat5"].as<long>()) / 1e5f;
      lon = (doc["lon5"].as<long>()) / 1e5f;
    }
    geoName     = doc["geoname"]     | geoName;
    geoFor      = doc["geofor"]      | geoFor;
    timezoneMin = doc["timezone"]    | timezoneMin;
    tz          = doc["tz"]          | tz;
    celsius     = doc["celsius"]     | celsius;
    hour12      = doc["hour12"]      | hour12;
    mile        = doc["mile"]        | mile;
    theme       = doc["theme"]       | theme;
    themeInterval = doc["themeInterval"] | themeInterval;
    faceMask    = doc["faces"]       | faceMask;
    brightness  = doc["brightness"]  | brightness;
    nightMode   = doc["nightmode"]   | nightMode;
    nightBrightness = doc["nightbrightness"] | nightBrightness;
    startHour   = doc["starttime"]   | startHour;
    stopHour    = doc["stoptime"]    | stopHour;
    color1      = doc["color1"]      | color1;
    color2      = doc["color2"]      | color2;
    color3      = doc["color3"]      | color3;
    return true;
  }

  bool save() {
    JsonDocument doc;
    doc["ssid"] = ssid;           doc["password"] = password;
    doc["city"] = city;           doc["ntp"] = ntp;
    doc["timezone"] = timezoneMin;
    // Integers, not floats: saves pulling a float writer into the file path.
    if (!isnan(lat) && !isnan(lon)) { doc["lat5"] = lroundf(lat * 1e5f); doc["lon5"] = lroundf(lon * 1e5f); }
    doc["geoname"] = geoName; doc["geofor"] = geoFor;
    doc["tz"] = tz;
    doc["celsius"] = celsius;     doc["hour12"] = hour12;   doc["mile"] = mile;
    doc["theme"] = theme;         doc["themeInterval"] = themeInterval;
    doc["faces"] = faceMask;
    doc["brightness"] = brightness;
    doc["nightmode"] = nightMode; doc["nightbrightness"] = nightBrightness;
    doc["starttime"] = startHour; doc["stoptime"] = stopHour;
    doc["color1"] = color1;       doc["color2"] = color2;   doc["color3"] = color3;
    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
  }
};

extern Config cfg;   // defined in main.cpp
