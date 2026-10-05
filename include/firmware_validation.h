#pragma once
#include <stddef.h>
#include <stdint.h>

// elf2bin.py writes these fields into uncompressed ESP8266 Arduino images.
// CRC covers the entire file with the length/CRC fields zeroed.
struct FirmwareValidation {
  static constexpr size_t prefixSize = 4120;
  uint8_t prefix[prefixSize] = {};
  size_t received = 0;
  uint32_t expectedSize = 0, expectedCrc = 0, crc = 0xffffffff;
  uint8_t lastByte = 0;

  void reset() {
    received = expectedSize = expectedCrc = 0;
    crc = 0xffffffff;
    lastByte = 0;
  }

  static uint32_t word(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
  }
  bool headerValid(size_t capacity) {
    expectedSize = word(prefix + 4112);
    expectedCrc = word(prefix + 4116);
    // Restrict OTA to the project's raw 4MB/DIO/40MHz Arduino image format.
    return prefix[0] == 0xe9 && prefix[2] == 2 && prefix[3] == 0x40 &&
           prefix[4096] == 0xe9 && prefix[4098] == 2 && prefix[4099] == 0x40 &&
           prefix[4097] == 5 && word(prefix + 4104) == 0x40201010 &&
           expectedSize > prefixSize && expectedSize <= capacity &&
           word(prefix + 4108) <= expectedSize - prefixSize;
  }
  void consume(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      if (received < prefixSize) prefix[received] = data[i];
      uint8_t value = received >= 4112 && received < 4120 ? 0 : data[i];
      crc ^= uint32_t(value) << 24;
      for (int bit = 0; bit < 8; ++bit)
        crc = (crc << 1) ^ ((crc & 0x80000000) ? 0x04c11db7 : 0);
      lastByte = data[i];
      ++received;
    }
  }
  bool complete() const {
    return received == expectedSize && crc == expectedCrc;
  }
};
