#include "storage.h"
#include <EEPROM.h>

uint32_t CalStorage::crc32_ieee(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = -(crc & 1U);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

bool CalStorage::begin() {
  EEPROM.begin(EEPROM_SIZE);
  return true;
}

bool CalStorage::save(const CalData &cal) {
  BlobV1 b{};
  b.magic   = MAGIC;
  b.version = VERSION;
  b.size    = sizeof(BlobV1);
  b.offset  = cal.offset;
  b.scale   = cal.scale;
  b.invert  = cal.invert ? 1 : 0;
  b.valid   = cal.valid ? 1 : 0;

  b.crc32 = 0;
  b.crc32 = crc32_ieee(reinterpret_cast<const uint8_t*>(&b),
                       sizeof(BlobV1) - sizeof(uint32_t));

  if (sizeof(BlobV1) > EEPROM_SIZE) return false;

  const uint8_t *p = reinterpret_cast<const uint8_t*>(&b);
  for (size_t i = 0; i < sizeof(BlobV1); i++) EEPROM.write(EEPROM_ADDR + (int)i, p[i]);
  EEPROM.commit();
  return true;
}

bool CalStorage::load(CalData &cal) {
  if (sizeof(BlobV1) > EEPROM_SIZE) return false;

  BlobV1 b{};
  uint8_t *p = reinterpret_cast<uint8_t*>(&b);
  for (size_t i = 0; i < sizeof(BlobV1); i++) p[i] = EEPROM.read(EEPROM_ADDR + (int)i);

  if (b.magic != MAGIC) return false;
  if (b.version != VERSION) return false;
  if (b.size != sizeof(BlobV1)) return false;

  uint32_t crc = b.crc32;
  b.crc32 = 0;
  uint32_t calc = crc32_ieee(reinterpret_cast<const uint8_t*>(&b),
                             sizeof(BlobV1) - sizeof(uint32_t));
  if (crc != calc) return false;

  cal.offset = b.offset;
  cal.scale  = b.scale;
  cal.invert = (b.invert != 0);
  cal.valid  = (b.valid != 0);
  return true;
}

bool CalStorage::reset() {
  for (int i = 0; i < (int)EEPROM_SIZE; i++) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  return true;
}
