#include "weather.h"
#include <algorithm>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

Weather wx;
Forecast fc;

static String urlEncode(const String& s) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  for (unsigned i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else { out += '%'; out += hex[(c >> 4) & 15]; out += hex[c & 15]; }
  }
  return out;
}

// GET a JSON document over plain HTTP. HTTP/1.0 keeps the body un-chunked so
// ArduinoJson can parse the raw stream; the filter keeps only the fields we use.
static bool httpJson(const String& url, JsonDocument& doc, JsonDocument* filter = nullptr) {
  if (WiFi.status() != WL_CONNECTED) { wx.error = F("Not connected"); return false; }
  WiFiClient client; HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);   // api.open-meteo.com takes ~1.2 s from Australia
  if (!http.begin(client, url)) { wx.error = F("Bad request"); return false; }
  int code = http.GET();
  if (code != 200) {
    http.end();
    if (code == 404) wx.error = F("City not found");
    else if (code < 0) wx.error = String(F("Weather service unreachable (")) + code + ")";
    else wx.error = String(F("Weather service error ")) + code;
    return false;
  }
  DeserializationError e = filter ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
                                  : deserializeJson(doc, http.getStream());
  http.end();
  if (e) { wx.error = F("Unreadable weather data"); return false; }
  return true;
}

// ---- Open-Meteo: no key; one request gives current conditions + 4 days ----------
// WMO weather code -> OpenWeatherMap icon code and wording. Written as a switch
// with F() strings: on the ESP8266 a table of literals would sit in RAM.
static const char* wmoIcon(int code) {
  if (code == 0) return "01";
  if (code == 1) return "02";
  if (code == 2) return "03";
  if (code == 45 || code == 48) return "50";
  if ((code >= 51 && code <= 57) || (code >= 80 && code <= 82)) return "09";
  if (code >= 61 && code <= 65) return "10";
  if (code == 66 || code == 67 || (code >= 71 && code <= 77) || code == 85 || code == 86) return "13";
  if (code >= 95) return "11";
  return "04";   // 3 = overcast, and anything unknown
}

static void wmoText(int code, String& main, String& desc) {
  switch (code) {
    case 0:  main = F("Clear"); desc = F("clear sky"); break;
    case 1:  main = F("Clear"); desc = F("mainly clear"); break;
    case 2:  main = F("Clouds"); desc = F("partly cloudy"); break;
    case 45: main = F("Fog"); desc = F("fog"); break;
    case 48: main = F("Fog"); desc = F("freezing fog"); break;
    case 51: main = F("Drizzle"); desc = F("light drizzle"); break;
    case 53: main = F("Drizzle"); desc = F("drizzle"); break;
    case 55: main = F("Drizzle"); desc = F("heavy drizzle"); break;
    case 56: case 57: main = F("Drizzle"); desc = F("freezing drizzle"); break;
    case 61: main = F("Rain"); desc = F("light rain"); break;
    case 63: main = F("Rain"); desc = F("rain"); break;
    case 65: main = F("Rain"); desc = F("heavy rain"); break;
    case 66: case 67: main = F("Rain"); desc = F("freezing rain"); break;
    case 71: main = F("Snow"); desc = F("light snow"); break;
    case 73: main = F("Snow"); desc = F("snow"); break;
    case 75: main = F("Snow"); desc = F("heavy snow"); break;
    case 77: main = F("Snow"); desc = F("snow grains"); break;
    case 80: main = F("Rain"); desc = F("light showers"); break;
    case 81: main = F("Rain"); desc = F("showers"); break;
    case 82: main = F("Rain"); desc = F("heavy showers"); break;
    case 85: main = F("Snow"); desc = F("snow showers"); break;
    case 86: main = F("Snow"); desc = F("heavy snow showers"); break;
    case 95: main = F("Thunderstorm"); desc = F("thunderstorm"); break;
    case 96: case 99: main = F("Thunderstorm"); desc = F("thunderstorm with hail"); break;
    default: main = F("Clouds"); desc = F("overcast"); break;
  }
}

// "Sydney" or "Sydney, AU" (an ISO country code narrows the search).
static bool geocode() {
  String name = cfg.city, cc;
  name.trim();
  int comma = name.lastIndexOf(',');
  if (comma > 0) { cc = name.substring(comma + 1); cc.trim(); cc.toUpperCase(); name = name.substring(0, comma); name.trim(); }
  if (name.length() < 2) { wx.error = F("Set a city"); return false; }
  String url = "http://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=" + urlEncode(name);
  if (cc.length() == 2) url += "&countryCode=" + cc;
  JsonDocument filter;   // results also carry long postcode lists
  JsonObject r = filter["results"].add<JsonObject>();
  r["name"] = true; r["latitude"] = true; r["longitude"] = true; r["country_code"] = true; r["admin1"] = true;
  JsonDocument doc;
  if (!httpJson(url, doc, &filter)) return false;
  JsonObject hit = doc["results"][0];
  if (hit.isNull()) { wx.error = F("City not found"); return false; }
  cfg.lat = hit["latitude"] | NAN;
  cfg.lon = hit["longitude"] | NAN;
  String label = (const char*)(hit["name"] | "");
  const char* region = hit["admin1"] | "";
  if (*region && label != region) label += String(", ") + region;
  const char* country = hit["country_code"] | "";
  if (*country) label += String(", ") + country;
  cfg.geoName = label;
  cfg.geoFor = cfg.city;
  cfg.save();   // once per city change, not per fetch
  return !isnan(cfg.lat) && !isnan(cfg.lon);
}

static int weekdayOf(const char* d) {   // "2026-10-05" -> 0 = Sunday (Sakamoto)
  if (strlen(d) < 10) return 0;
  int y = atoi(d), m = atoi(d + 5), day = atoi(d + 8);
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 1 || m > 12) return 0;
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + day) % 7;
}

// "-33.8679" without String(float), which drags in dtostrf.
static String coord(float v) {
  long q = lroundf(v * 1e4f);
  String out = q < 0 ? "-" : "";
  q = labs(q);
  char frac[6]; snprintf(frac, sizeof(frac), "%04ld", q % 10000);
  return out + String(q / 10000) + "." + frac;
}

static bool fetchOpenMeteo() {
  if (cfg.geoFor != cfg.city || isnan(cfg.lat) || isnan(cfg.lon)) {
    if (!geocode()) return false;
  }
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + coord(cfg.lat) +
               "&longitude=" + coord(cfg.lon) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code,is_day"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
               "&forecast_days=4&timezone=auto&wind_speed_unit=ms";
  JsonDocument doc;   // ~1 KB response
  if (!httpJson(url, doc)) return false;
  JsonObject cur = doc["current"];
  JsonObject daily = doc["daily"];
  if (cur.isNull() || daily.isNull()) { wx.error = F("Unreadable weather data"); return false; }

  int code = cur["weather_code"] | 3;
  bool day = (cur["is_day"] | 1) != 0;
  wx.temp = cur["temperature_2m"] | 0.0f;
  wx.feels = cur["apparent_temperature"] | wx.temp;
  wx.humidity = cur["relative_humidity_2m"] | 0;
  wx.wind = cur["wind_speed_10m"] | 0.0f;
  JsonArray dates = daily["time"], codes = daily["weather_code"], his = daily["temperature_2m_max"],
            los = daily["temperature_2m_min"], pops = daily["precipitation_probability_max"];
  auto minutes = [](const char* iso) -> int16_t {   // "2026-10-06T05:41"
    return iso && strlen(iso) >= 16 ? atoi(iso + 11) * 60 + atoi(iso + 14) : -1;
  };
  wx.sunrise = minutes(daily["sunrise"][0] | "");
  wx.sunset = minutes(daily["sunset"][0] | "");
  wx.tempMax = his[0] | wx.temp;
  wx.tempMin = los[0] | wx.temp;
  wmoText(code, wx.main, wx.desc);
  wx.icon = String(wmoIcon(code)) + (day ? "d" : "n");
  int comma = cfg.geoName.indexOf(',');
  wx.cityName = comma > 0 ? cfg.geoName.substring(0, comma) : cfg.geoName;
  wx.valid = true; wx.error = ""; wx.version++;
  wx.lastSuccess = millis();

  Forecast next;
  for (size_t i = 0; i < dates.size() && next.count < FORECAST_DAYS; ++i) {
    ForecastDay& d = next.days[next.count++];
    d.wday = weekdayOf(dates[i] | "");
    d.today = i == 0;
    d.hi = his[i] | 0.0f;
    d.lo = los[i] | 0.0f;
    d.pop = pops[i] | 0;
    d.icon = String(wmoIcon(codes[i] | 3)) + "d";
  }
  next.valid = next.count > 0;
  next.lastFetch = millis();
  next.version = fc.version + 1;
  fc = next;
  return true;
}

bool fetchWeather() {
  wx.lastFetch = millis();
  return fetchOpenMeteo();
}
