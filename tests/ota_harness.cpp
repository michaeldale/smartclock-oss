#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "firmware_validation.h"
struct String : std::string {
  using std::string::string;
  size_t length() const { return size(); }
};
enum { U_FLASH, HTTP_POST, UPLOAD_FILE_START, UPLOAD_FILE_WRITE,
       UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status; uint8_t* buf; size_t currentSize; String filename; };
struct MockServer {
  HTTPUpload up{};
  int code = 0;
  String message;
  HTTPUpload& upload() { return up; }
  void sendHeader(const char*, const char*) {}
  void send(int status, const char*, const String& value) { code = status; message = value; }
} server;
struct MockESP {
  unsigned restarts = 0;
  uint32_t getFreeSketchSpace() { return 1500000; }
  void restart() { ++restarts; }
} ESP;
struct MockUpdate {
  bool running = false, committed = false, failBegin = false, failWrite = false, failEnd = false;
  size_t expected = 0;
  std::vector<uint8_t> staged;
  bool begin(size_t size, int command) {
    assert(command == U_FLASH);
    if (failBegin) return false;
    assert(!running); running = true; expected = size; staged.clear(); return true;
  }
  size_t write(uint8_t* data, size_t size) {
    if (!running || failWrite) return 0;
    assert(staged.size() + size <= expected);
    staged.insert(staged.end(), data, data + size); return size;
  }
  bool end(bool allowPartial) {
    assert(!allowPartial);
    committed = running && staged.size() == expected && !failEnd;
    running = false; return committed;
  }
  bool isRunning() { return running; }
} Update;
void delay(unsigned) {}
void yield() {}
void safeRestart() { ESP.restart(); }

// Simulated EEPROM sector: persists across "boots", counts real flash commits.
constexpr size_t SPI_FLASH_SEC_SIZE = 4096;
struct MockEEPROM {
  std::vector<uint8_t> flash = std::vector<uint8_t>(SPI_FLASH_SEC_SIZE, 0xff), ram;
  bool dirty = false; unsigned commits = 0;
  void begin(size_t size) { assert(size == SPI_FLASH_SEC_SIZE); ram = flash; dirty = false; }
  uint8_t read(int a) { return ram.at(a); }
  void write(int a, uint8_t v) { if (ram.at(a) != v) { ram[a] = v; dirty = true; } }
  void end() { if (dirty) { flash = ram; ++commits; } ram.clear(); }
} EEPROM;

// The runner inserts the power-cycle counter from src/main.cpp here.
// CYCLE_IMPLEMENTATION

// The runner inserts the actual OTA implementation from src/main.cpp here.
// OTA_IMPLEMENTATION

void reset() {
  assert(!Update.running);
  server = MockServer{}; ESP = MockESP{}; Update = MockUpdate{};
  otaStarted = otaReady = otaFailed = false; otaFiles = 0; otaError = "";
  firmware.reset();
}
void event(int status, uint8_t* data = nullptr, size_t size = 0) {
  server.up.status = status; server.up.buf = data; server.up.currentSize = size;
  handleOtaUpload();
}
void upload(std::vector<uint8_t>& image, size_t chunk = 2048) {
  event(UPLOAD_FILE_START);
  for (size_t offset = 0; offset < image.size(); offset += chunk)
    event(UPLOAD_FILE_WRITE, image.data() + offset, std::min(chunk, image.size() - offset));
  event(UPLOAD_FILE_END);
}
void rejected() {
  finishOta();
  assert(server.code == 400 && !Update.committed && !Update.running && ESP.restarts == 0);
}
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream file(argv[1], std::ios::binary);
  std::vector<uint8_t> image{std::istreambuf_iterator<char>(file), {}};
  assert(image.size() > FirmwareValidation::prefixSize);
  for (size_t chunk : {size_t(1), size_t(511), size_t(2048), size_t(4096), image.size()}) {
    reset(); server.up.filename = "clock-lfs.bin"; upload(image, chunk);
    assert(otaReady && !Update.committed && ESP.restarts == 0);
    assert(Update.staged.size() == image.size() - 1);
    finishOta();
    assert(server.code == 200 && Update.committed && ESP.restarts == 1 && Update.staged == image);
  }
  for (size_t length : {size_t(0), size_t(100), size_t(4119), size_t(4120), image.size() - 1}) {
    reset(); auto partial = image; partial.resize(length); upload(partial); rejected();
  }
  reset(); auto corrupt = image; corrupt[5000] ^= 1; upload(corrupt); rejected();
  reset(); auto oversize = image; oversize.push_back(0); upload(oversize); rejected();
  reset(); auto wrongMode = image; wrongMode[2] = 0; upload(wrongMode); rejected();
  reset(); auto badLength = image; badLength[4115] = 0xff; upload(badLength); rejected();
  reset(); auto filesystem = image; filesystem[0] = 0; server.up.filename = "littlefs.bin";
  upload(filesystem); rejected();
  reset(); upload(image); event(UPLOAD_FILE_START); rejected();
  reset(); finishOta(); assert(server.code == 400 && ESP.restarts == 0);
  reset(); event(UPLOAD_FILE_START); event(UPLOAD_FILE_WRITE, image.data(), 8192);
  event(UPLOAD_FILE_ABORTED); assert(!Update.running && !Update.committed);
  upload(image); finishOta(); assert(Update.committed && ESP.restarts == 1);
  reset(); upload(image); event(UPLOAD_FILE_ABORTED);
  assert(!Update.running && !Update.committed && ESP.restarts == 0);
  upload(image); finishOta(); assert(Update.committed && ESP.restarts == 1);
  reset(); Update.failBegin = true; upload(image); rejected();
  reset(); Update.failWrite = true; upload(image); rejected();
  reset(); Update.failEnd = true; upload(image); rejected();
  // Power-cycle recovery: boots shorter than the window count up; the third triggers.
  EEPROM = MockEEPROM{}; EEPROM.flash[0x10] = 0x5a;   // a stock byte elsewhere in the sector
  assert(!countPowerCycle() && !countPowerCycle() && countPowerCycle());
  assert(!countPowerCycle());                        // trigger resets the count
  clearPowerCycles(); assert(!countPowerCycle() && !countPowerCycle());
  clearPowerCycles(); unsigned commits = EEPROM.commits;
  clearPowerCycles(); assert(EEPROM.commits == commits); // no erase when already clear
  assert(!countPowerCycle()); clearPowerCycles();   // a normal boot never accumulates
  assert(!countPowerCycle()); clearPowerCycles(); assert(!countPowerCycle());
  EEPROM.flash[CYCLE_ADDR + 4] = 200;                // corrupt count is clamped, not looped
  assert(countPowerCycle());
  assert(EEPROM.flash[0x10] == 0x5a);                // stock byte preserved
  std::cout << "PASS: power-cycle counter (trigger on 3rd short boot, reset, clamp, no needless erase, sector preserved)\n";
  std::cout << "PASS: complete/chunked/renamed, truncated, corrupt, oversized, invalid header, filesystem, multiple-file, empty, abort/retry, and updater failure cases\n";
}
