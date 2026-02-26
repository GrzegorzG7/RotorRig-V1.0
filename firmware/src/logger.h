#pragma once
#include <Arduino.h>
#include "esc_bdshot.h"
#include "sensors_ina226.h"
#include "sensors_hx711.h"

struct Meta {
  String test_id = "NA";
  String motor_id = "NA";
  int kv = -1;
  String prop = "NA";
  int battery_s = -1;
  String esc_fw = "NA";
  uint8_t pole_pairs = 7;
};

struct LogFrame {
  uint32_t t_ms = 0;

  // meta
  String test_id, motor_id, prop, esc_fw;
  int kv = -1;
  int battery_s = -1;
  uint8_t pole_pairs = 7;

  int step_id = -1;
  float throttle_pct = NAN;
  float step_time_s = NAN;
  int is_steady = 0;

  // telemetry
  uint32_t erpm = 0;
  uint32_t rpm = 0;

  float v_bus_V = NAN;
  float i_A = NAN;
  float p_in_W = NAN;

  float thrust_N = NAN;
  float thrust_g = NAN;

  float eff_g_per_W = NAN;
  float eff_N_per_W = NAN;
  float eff_g_per_A = NAN;

  float bdshot_err_pct = NAN;

  String notes = "OK";
};

class Logger {
public:
  void printCsv(const LogFrame& f);
};
