#pragma once
#include <Arduino.h>

// Optional integrations, each off by default (settings page, Integrations card):
//  - Custom faces: JSON-described faces over HTTP (/api/faces) and MQTT, with
//    placeholders ({time}, {temp}, ...) and variables pushed separately.
//  - Stock SD Pro photo API (/theme/*, /photo/*) so geekmagic-hacs can push
//    Home Assistant dashboards as photos. Those endpoints need no sign-in.
//  - MQTT with Home Assistant discovery.
// See docs/specifications/integrations-api.md.

void integrationsRoutes();          // registers the HTTP endpoints
void integrationsLoop();            // MQTT connection, state publishing
void integrationsSettingsChanged(); // MQTT settings may have changed: reconnect

// Custom faces, for faces.cpp and the rotation.
constexpr size_t FACE_JSON_MAX = 4096;
uint32_t customFacesVersion();      // bumped whenever a face is saved or deleted
int customFaceCount();
String customFaceAt(int index);     // "" when out of range
bool customFaceExists(const String& name);
String faceVar(const String& key);  // a variable pushed with /api/faces/vars, or ""

// Photos: per-photo slideshow switch, shared with the stock-compatible API.
bool photoEnabled(const String& name);
