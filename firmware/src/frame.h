#pragma once
#include <Arduino.h>

// Single 10Hz frame used for status + CSV logging.
// Keep NaN defaults where measurement may be unavailable.
struct Frame {
  // core time
  uint32_t t_ms = 0;

  // metadata-ish runtime fields (filled elsewhere if needed)
  int32_t step_id = -1;
  float throttle_pct = 0.0f;
  float step_time_s = NAN;
  uint8_t is_steady = 0;

  // ESC telemetry
  uint32_t erpm = 0;
  uint32_t rpm = 0;
  float bdshot_err_pct = 100.0f;

  // INA226
  float v_bus_V = NAN;
  float i_A = NAN;
  float p_in_W = NAN;

  // HX711 thrust
  float thrust_N = NAN;
  float thrust_g = NAN;

  // efficiencies
  float eff_g_per_W = NAN;
  float eff_N_per_W = NAN;
  float eff_g_per_A = NAN;

  // diagnostics
  int32_t hx_noise_pp = -1; // peak-to-peak raw in the last 100ms window, -1 = unknown
};
