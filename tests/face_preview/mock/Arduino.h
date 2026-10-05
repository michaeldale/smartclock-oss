// Host stand-in for the parts of Arduino.h that src/faces.cpp uses.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define PROGMEM
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
inline uint8_t pgm_read_byte(const void* p) { return *static_cast<const uint8_t*>(p); }

template <class T, class L, class H> T constrain(T x, L lo, H hi) { return x < lo ? lo : x > hi ? hi : x; }
uint32_t millis();
void delay(unsigned long ms);

class String {
  std::string s;
 public:
  String() = default;
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& v) : s(v) {}
  String(int v) : s(std::to_string(v)) {}
  unsigned length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
  bool isEmpty() const { return s.empty(); }
  char operator[](unsigned i) const { return i < s.size() ? s[i] : 0; }
  String substring(unsigned a, unsigned b = ~0u) const {
    if (a > s.size()) return String();
    return String(s.substr(a, b == ~0u ? std::string::npos : b - a));
  }
  long toInt() const { return atol(s.c_str()); }
  bool endsWith(const String& x) const { return s.size() >= x.s.size() && s.compare(s.size() - x.s.size(), x.s.size(), x.s) == 0; }
  bool startsWith(const String& x) const { return s.compare(0, x.s.size(), x.s) == 0; }
  void setCharAt(unsigned i, char c) { if (i < s.size()) s[i] = c; }
  void toUpperCase() { for (auto& c : s) c = toupper((unsigned char)c); }
  void toLowerCase() { for (auto& c : s) c = tolower((unsigned char)c); }
  void remove(unsigned i) { if (i < s.size()) s.erase(i); }
  void remove(unsigned i, unsigned n) { if (i < s.size()) s.erase(i, n); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  bool operator==(const String& o) const { return s == o.s; }
  friend String operator+(const String& a, const String& b) { return String(a.s + b.s); }
  friend String operator+(const String& a, const char* b) { return String(a.s + b); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
};
