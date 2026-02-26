#include "csv.h"
#include "esc_bdshot.h"

static void printFieldStr(const String& s) {
  if (s.length() == 0) Serial.print("NA");
  else Serial.print(s);
}

static void printFieldFloat(float v, int prec = 6) {
  if (!isfinite(v)) Serial.print("NaN");
  else Serial.print(v, prec);
}

static void printFieldInt(long v) {
  Serial.print(v);
}

void printCsvFrame(const Frame& f, const Meta& meta, const EscBdshot& esc, const String& notes) {
  // 24 columns, no header:
  // t_ms, test_id, motor_id, kv, prop, battery_s, esc_fw, pole_pairs, step_id, throttle_pct,
  // step_time_s, is_steady, eRPM, RPM, V_bus_V, I_A, P_in_W, thrust_N, thrust_g,
  // eff_g_per_W, eff_N_per_W, eff_g_per_A, bdshot_err_pct, notes

  printFieldInt((long)f.t_ms); Serial.print(',');

  printFieldStr(meta.test_id); Serial.print(',');
  printFieldStr(meta.motor_id); Serial.print(',');

  printFieldInt(meta.kv); Serial.print(',');
  printFieldStr(meta.prop); Serial.print(',');
  printFieldInt(meta.battery_s); Serial.print(',');

  printFieldStr(meta.esc_fw); Serial.print(',');
  printFieldInt((long)meta.pole_pairs); Serial.print(',');

  printFieldInt((long)f.step_id); Serial.print(',');
  printFieldFloat(f.throttle_pct, 2); Serial.print(',');

  printFieldFloat(f.step_time_s, 3); Serial.print(',');
  printFieldInt((long)f.is_steady); Serial.print(',');

  printFieldInt((long)f.erpm); Serial.print(',');
  printFieldInt((long)f.rpm); Serial.print(',');

  printFieldFloat(f.v_bus_V, 6); Serial.print(',');
  printFieldFloat(f.i_A, 6); Serial.print(',');
  printFieldFloat(f.p_in_W, 6); Serial.print(',');

  printFieldFloat(f.thrust_N, 6); Serial.print(',');
  printFieldFloat(f.thrust_g, 6); Serial.print(',');

  printFieldFloat(f.eff_g_per_W, 6); Serial.print(',');
  printFieldFloat(f.eff_N_per_W, 6); Serial.print(',');
  printFieldFloat(f.eff_g_per_A, 6); Serial.print(',');

  printFieldFloat(f.bdshot_err_pct, 6); Serial.print(',');

  printFieldStr(notes);
  Serial.println();
}
