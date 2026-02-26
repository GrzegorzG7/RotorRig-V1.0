#include "sensors_ina226.h"
#include "cfg.h"
#include <Wire.h>
#include <INA226.h>

// Your INA226 lib: INA226(address, TwoWire*) + begin()
static INA226* g_ina = nullptr;

bool SensorsIna226::begin(uint8_t addr, float shunt_ohms, float expected_max_current_A) {
  addr_ = addr;
  shunt_ohms_ = shunt_ohms;
  expected_max_current_A_ = expected_max_current_A;

  Wire.begin();
  Wire.setClock(400000);

  if (g_ina) { delete g_ina; g_ina = nullptr; }
  g_ina = new INA226(addr_, &Wire);

  return g_ina->begin();
}

InaSample SensorsIna226::read() {
  InaSample s;
  if (!g_ina) return s;

  const float v = g_ina->getBusVoltage(); // V

  if (isnan(v) || v < VBAT_PRESENT_THRESHOLD_V) {
    s.present = false;
    s.v_bus_V = v;
    s.i_A = NAN;
    s.p_W = NAN;
    return s;
  }

  s.present = true;
  s.v_bus_V = v;

  // Prefer current from shunt voltage: I = Vshunt / Rshunt
  const float vsh = g_ina->getShuntVoltage(); // (should be in V in this lib)
  if (isnan(vsh)) {
    s.i_A = NAN;
    s.p_W = NAN;
    return s;
  }

  const float i = vsh / shunt_ohms_;
  s.i_A = i;
  s.p_W = v * i;
  return s;
}
