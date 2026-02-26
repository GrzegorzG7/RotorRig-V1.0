#pragma once
#include <Arduino.h>

struct CalData {
  int32_t offset = 0;
  float   scale  = 1.0f;   // abs(counts/g)
  bool    invert = false;
  bool    valid  = false;
};

class CalStorage {
public:
  bool begin();
  bool save(const CalData &cal);
  bool load(CalData &cal);
  bool reset();

private:
  static constexpr uint32_t MAGIC   = 0x48583731UL; // "HX71"
  static constexpr uint16_t VERSION = 1;
  static constexpr size_t EEPROM_SIZE = 128;
  static constexpr int EEPROM_ADDR = 0;

  struct BlobV1 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    int32_t  offset;
    float    scale;
    uint8_t  invert;
    uint8_t  valid;
    uint8_t  rsvd0;
    uint8_t  rsvd1;
    uint32_t crc32;
  };

  static uint32_t crc32_ieee(const uint8_t *data, size_t len);
};
