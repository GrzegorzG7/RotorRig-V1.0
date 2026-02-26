#include "logger.h"

static void printFloatOrNaN(float v, int decimals = 6) {
  if (isnan(v) || isinf(v)) {
    Serial.print("NaN");
  } else {
    Serial.print(v, decimals);
  }
}

void Logger::printCsv(const LogFrame& f) {
  // 24 columns, no header, fixed order
  Serial.print(f.t_ms); Serial.print(',');

  Serial.print(f.test_id); Serial.print(',');
  Serial.print(f.motor_id); Serial.print(',');
  Serial.print(f.kv); Serial.print(',');
  Serial.print(f.prop); Serial.print(',');
  Serial.print(f.battery_s); Serial.print(',');
  Serial.print(f.esc_fw); Serial.print(',');
  Serial.print((int)f.pole_pairs); Serial.print(',');

  Serial.print(f.step_id); Serial.print(',');
  printFloatOrNaN(f.throttle_pct, 6); Serial.print(',');
  printFloatOrNaN(f.step_time_s, 6); Serial.print(',');
  Serial.print(f.is_steady); Serial.print(',');

  Serial.print(f.erpm); Serial.print(',');
  Serial.print(f.rpm); Serial.print(',');

  printFloatOrNaN(f.v_bus_V, 6); Serial.print(',');
  printFloatOrNaN(f.i_A, 6); Serial.print(',');
  printFloatOrNaN(f.p_in_W, 6); Serial.print(',');

  printFloatOrNaN(f.thrust_N, 6); Serial.print(',');
  printFloatOrNaN(f.thrust_g, 6); Serial.print(',');

  printFloatOrNaN(f.eff_g_per_W, 6); Serial.print(',');
  printFloatOrNaN(f.eff_N_per_W, 6); Serial.print(',');
  printFloatOrNaN(f.eff_g_per_A, 6); Serial.print(',');

  printFloatOrNaN(f.bdshot_err_pct, 6); Serial.print(',');

  Serial.print(f.notes);
  Serial.print("\n");
}
