#pragma once
#include <Arduino.h>

struct InaSample {
  bool present = false;
  float v_bus_V = NAN;
  float i_A = NAN;
  float p_W = NAN;
};

class SensorsIna226 {
public:
  // MUST match main.cpp call
  bool begin(uint8_t addr, float shunt_ohms, float expected_max_current_A);
  InaSample read();

  uint8_t addr() const { return addr_; }

private:
  uint8_t addr_ = 0x40;
  float shunt_ohms_ = 0.001f;
  float expected_max_current_A_ = 60.0f;
};
