#pragma once
#include <Arduino.h>
#include <time.h>

// Weather state shared by the web API and the clock faces, filled from Open-Meteo.
// Icons use OpenWeatherMap-style codes ("10d"), mapped from WMO weather codes.
struct Weather {
  bool valid = false;
  float temp = 0, feels = 0, tempMin = 0, tempMax = 0, wind = 0;   // metric: C, m/s
  int humidity = 0, pressure = 0, clouds = 0;
  String main, desc, icon, cityName;
  int16_t sunrise = -1, sunset = -1;   // today, minutes after local midnight; -1 unknown
  String error;                        // last failure, for the page and the faces
  uint32_t lastFetch = 0;              // millis() of the last attempt
  uint32_t lastSuccess = 0;            // millis() of the last successful fetch
  uint32_t version = 0;                // bumped on every successful fetch
};

constexpr int FORECAST_DAYS = 4;
struct ForecastDay {
  int wday = 0;          // 0 = Sunday
  bool today = false;
  float lo = 0, hi = 0;  // C
  int pop = 0;           // highest chance of precipitation, %
  String icon;
};
struct Forecast {
  bool valid = false;
  int count = 0;
  ForecastDay days[FORECAST_DAYS];
  uint32_t lastFetch = 0;
  uint32_t version = 0;
};

extern Weather wx;
extern Forecast fc;

// Current conditions and the forecast in one request, geocoding the city first
// if it changed.
bool fetchWeather();
