// Host stand-in for LittleFS: "/photo" maps to the directory in $PHOTO_DIR.
#pragma once
#include <Arduino.h>
#include <filesystem>
#include <vector>

enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };

class File {
  FILE* f = nullptr;
 public:
  File() = default;
  explicit File(FILE* h) : f(h) {}
  explicit operator bool() const { return f != nullptr; }
  size_t read(uint8_t* buf, size_t n) { return f ? fread(buf, 1, n, f) : 0; }
  int read() { int c = f ? fgetc(f) : EOF; return c == EOF ? -1 : c; }   // ArduinoJson reader
  size_t readBytes(char* buf, size_t n) { return f ? fread(buf, 1, n, f) : 0; }
  bool seek(long n, SeekMode m) { return f && fseek(f, n, m == SeekSet ? SEEK_SET : m == SeekCur ? SEEK_CUR : SEEK_END) == 0; }
  void close() { if (f) fclose(f); f = nullptr; }
};

class Dir {
  std::vector<std::filesystem::directory_entry> items;
  size_t i = 0;
 public:
  explicit Dir(std::vector<std::filesystem::directory_entry> v = {}) : items(std::move(v)) {}
  bool next() { if (i < items.size()) { ++i; return true; } return false; }
  bool isFile() const { return items[i - 1].is_regular_file(); }
  String fileName() const { return String(items[i - 1].path().filename().string()); }
  size_t fileSize() const { return items[i - 1].file_size(); }
};

struct LittleFSClass {
  static std::string env(const char* name) { const char* d = getenv(name); return d ? d : ""; }
  static std::string root() { return env("PHOTO_DIR"); }
  static std::string host(const String& p) {   // /photo -> $PHOTO_DIR, /faces -> $FACES_DIR
    std::string s = p.c_str();
    if (s.rfind("/photo", 0) == 0 && !root().empty()) return root() + s.substr(6);
    if (s.rfind("/faces", 0) == 0 && !env("FACES_DIR").empty()) return env("FACES_DIR") + s.substr(6);
    return std::string();
  }
  File open(const String& path, const char*) { std::string h = host(path); return File(h.empty() ? nullptr : fopen(h.c_str(), "rb")); }
  Dir openDir(const char* path) {
    std::string h = host(path);
    std::vector<std::filesystem::directory_entry> v;
    std::error_code ec;
    if (!h.empty()) for (auto& e : std::filesystem::directory_iterator(h, ec)) v.push_back(e);
    std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.path() < b.path(); });
    return Dir(v);
  }
};
extern LittleFSClass LittleFS;
