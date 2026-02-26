#include "cli.h"
#include <Wire.h>

#include "esc_bdshot.h"
#include "sensors_hx711.h"
#include "sensors_ina226.h"
#include "autotest.h"
#include "meta.h"

static float parseFloatSafe(const String& s, float def = NAN) {
  char* endp = nullptr;
  float v = strtof(s.c_str(), &endp);
  if (endp == s.c_str()) return def;
  return v;
}

static long parseLongSafe(const String& s, long def = -1) {
  char* endp = nullptr;
  long v = strtol(s.c_str(), &endp, 10);
  if (endp == s.c_str()) return def;
  return v;
}

static void printFinite(float v, int prec, const char* suffix = "") {
  if (!isfinite(v)) Serial.print("NaN");
  else Serial.print(v, prec);
  if (suffix && suffix[0]) Serial.print(suffix);
}

void CLI::begin() { buf_.reserve(128); }

void CLI::bind(EscBdshot* esc, SensorsHx711* hx, SensorsIna226* ina, Meta* meta, AutoTest* at) {
  esc_ = esc;
  hx_ = hx;
  ina_ = ina;
  meta_ = meta;
  at_ = at;
}

void CLI::setLive(float thrust_g, float thrust_N,
                  float vbus_V, float i_A, float p_W,
                  uint32_t erpm, uint32_t rpm, float bdshot_err_pct,
                  int32_t hx_raw, int32_t hx_offset, float hx_scale,
                  bool hx_cal_valid, bool hx_inverted,
                  int32_t hx_noise_pp) {
  st_thrust_g_ = isfinite(thrust_g) ? thrust_g : NAN;
  st_thrust_N_ = isfinite(thrust_N) ? thrust_N : NAN;

  st_vbus_V_ = isfinite(vbus_V) ? vbus_V : NAN;
  st_i_A_    = isfinite(i_A) ? i_A : NAN;
  st_p_W_    = isfinite(p_W) ? p_W : NAN;

  st_erpm_ = erpm;
  st_rpm_  = rpm;
  st_bdshot_err_pct_ = isfinite(bdshot_err_pct) ? bdshot_err_pct : 100.0f;

  st_hx_raw_       = hx_raw;
  st_hx_offset_    = hx_offset;
  st_hx_scale_     = isfinite(hx_scale) ? hx_scale : NAN;
  st_hx_cal_valid_ = hx_cal_valid;
  st_hx_inverted_  = hx_inverted;
  st_hx_noise_pp_  = hx_noise_pp;
}

static void toLowerInPlace(String& s) { s.toLowerCase(); }

void CLI::tick() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (buf_.length() > 0) handleLine(buf_);
      buf_ = "";
    } else {
      if (buf_.length() < 200) buf_ += c;
    }
  }

  // non-blocking sequencer for CORE2 autotest
  serviceAutotestSequence();

  // soft-stop service (runs until fully stopped)
  serviceSoftStop();
}

// === SOFT STOP ===
void CLI::beginSoftStop(const char* reason_tag) {
  stop_reason_ = reason_tag ? reason_tag : "STOP";
  stop_active_ = true;
  stop_t0_ms_ = (uint32_t)millis();

  if (esc_) {
    // Smooth ramp to 0% to avoid abrupt torque change / spikes
    esc_->setTargetThrottlePct(0.0f, stop_ramp_s_);
  }

  Serial.print("OK STOPPING ramp_s=");
  printFinite(stop_ramp_s_, 2, "");
  Serial.print(" cut_pct=");
  printFinite(stop_cut_pct_, 2, "");
  Serial.print(" reason=");
  Serial.println(stop_reason_);
}

void CLI::serviceSoftStop() {
  if (!stop_active_) return;
  if (!esc_) {
    // nothing we can do; just close logs
    stop_active_ = false;
    if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
    Serial.println("OK STOP");
    return;
  }

  const uint32_t now = (uint32_t)millis();
  const uint32_t age = (uint32_t)(now - stop_t0_ms_);

  const float cur = esc_->currentThrottlePct();
  const bool low_enough = (cur <= stop_cut_pct_);
  const bool timed_out  = (age >= stop_timeout_ms_);

  if (low_enough || timed_out) {
    // Final disarm only when already near 0 (or timeout)
    esc_->stopNow();
    stop_active_ = false;

    if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
    Serial.println("OK STOP");
  }
}

void CLI::handleLine(const String& line) {
  const int maxTok = 16;
  String tok[maxTok];
  int n = 0;

  int i = 0;
  while (i < (int)line.length() && n < maxTok) {
    while (i < (int)line.length() && line[i] == ' ') i++;
    if (i >= (int)line.length()) break;
    int j = i;
    while (j < (int)line.length() && line[j] != ' ') j++;
    tok[n++] = line.substring(i, j);
    i = j;
  }
  if (n == 0) return;

  String cmd = tok[0];
  toLowerInPlace(cmd);

  if (cmd == "help") {
    Serial.println("CMDS: HELP, STATUS, SETMETA ..., LOG <0|1>, START, STOP, ESTOP");
    Serial.println("      STOPRAMP <sec>");
    Serial.println("      THROTTLE <pct>, TARE, CAL <mass_g>, CALTRIM <mass_g>");
    Serial.println("      AUTOTEST <core|core2|stop> [gap_s], I2CSCAN");
    Serial.println("      SAVE, LOAD, RESETCAL");
    return;
  }

  if (cmd == "status") { printStatus(); return; }

  if (cmd == "stopramp") {
    if (n < 2) { Serial.println("ERR stopramp <sec>"); return; }
    float s = parseFloatSafe(tok[1], NAN);
    if (!isfinite(s)) { Serial.println("ERR stopramp"); return; }
    // sane bounds
    if (s < 0.2f) s = 0.2f;
    if (s > 8.0f) s = 8.0f;
    stop_ramp_s_ = s;
    Serial.print("OK STOPRAMP ");
    printFinite(stop_ramp_s_, 2, " s\n");
    return;
  }

  if (cmd == "log") {
    if (n < 2) { Serial.println("ERR log <0|1>"); return; }
    long v = parseLongSafe(tok[1], -1);
    if (v != 0 && v != 1) { Serial.println("ERR log <0|1>"); return; }
    csv_on_ = (v == 1);
    Serial.println(csv_on_ ? "OK LOG 1" : "OK LOG 0");
    return;
  }

  if (cmd == "tare") {
    if (!hx_) { Serial.println("ERR TARE"); return; }
    hx_->tareTrimStart(200, 20);
    Serial.println("OK TARE (trim 200,20%)");
    return;
  }

  if (cmd == "cal") {
    if (n < 2) { Serial.println("ERR cal <mass_g>"); return; }
    float m = parseFloatSafe(tok[1], NAN);
    if (!hx_ || isnan(m) || m <= 0.0f) { Serial.println("ERR cal"); return; }
    bool ok = hx_->calibrateWithMass(m);
    Serial.println(ok ? "OK CAL" : "ERR CAL");
    return;
  }

  if (cmd == "caltrim") {
    if (n < 2) { Serial.println("ERR caltrim <mass_g>"); return; }
    float m = parseFloatSafe(tok[1], NAN);
    if (!hx_ || isnan(m) || m <= 0.0f) { Serial.println("ERR caltrim"); return; }
    hx_->calTrimStart(m, 200, 20);
    Serial.println("OK CALTRIM (200,20%)");
    return;
  }

  if (cmd == "save") {
    if (!hx_) { Serial.println("ERR SAVE"); return; }
    Serial.println(hx_->saveCal() ? "OK SAVE" : "ERR SAVE");
    return;
  }

  if (cmd == "load") {
    if (!hx_) { Serial.println("ERR LOAD"); return; }
    Serial.println(hx_->loadCal() ? "OK LOAD" : "ERR LOAD");
    return;
  }

  if (cmd == "resetcal") {
    if (!hx_) { Serial.println("ERR RESETCAL"); return; }
    hx_->resetCal();
    Serial.println("OK RESETCAL");
    return;
  }

  if (cmd == "setmeta") {
    if (!meta_) { Serial.println("ERR meta"); return; }
    if (n < 8) {
      Serial.println("ERR setmeta <test_id> <motor_id> <kv> <prop> <battery_s> <esc_fw> <pole_pairs>");
      return;
    }
    meta_->test_id = tok[1];
    meta_->motor_id = tok[2];
    meta_->kv = (int)parseLongSafe(tok[3], -1);
    meta_->prop = tok[4];
    meta_->battery_s = (int)parseLongSafe(tok[5], -1);
    meta_->esc_fw = tok[6];
    meta_->pole_pairs = (uint8_t)parseLongSafe(tok[7], 7);
    if (esc_) esc_->setPolePairs(meta_->pole_pairs);
    Serial.println("OK SETMETA");
    return;
  }

  if (cmd == "start") {
    armed_ = true;
    stop_active_ = false; // cancel any pending stop
    if (esc_) esc_->clearFailsafe();
    Serial.println("OK START");
    return;
  }

  // === ESTOP (hard) ===
  if (cmd == "estop") {
    armed_ = false;
    at_mode_ = 0;
    at_seq_active_ = false;
    at_seq_phase_ = 0;
    if (at_) at_->stop();
    stop_active_ = false;

    if (esc_) esc_->stopNow();

    if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
    Serial.println("OK ESTOP");
    return;
  }

  // === STOP (soft) ===
  if (cmd == "stop")  {
    armed_ = false;
    at_mode_ = 0;
    at_seq_active_ = false;
    at_seq_phase_ = 0;
    if (at_) at_->stop();

    // do NOT stopNow immediately – start soft stop sequence
    if (!stop_active_) beginSoftStop("STOP");

    return;
  }

  if (cmd == "autotest") {
    if (!at_ || !esc_) { Serial.println("ERR autotest"); return; }
    if (!armed_) { Serial.println("ERR NOT_ARMED (use start)"); return; }
    if (n < 2) { Serial.println("ERR autotest <core|core2|stop> [gap_s]"); return; }
    String sub = tok[1];
    toLowerInPlace(sub);

    if (sub == "stop") {
      at_mode_ = 0;
      at_seq_active_ = false;
      at_seq_phase_ = 0;
      at_->stop();

      // soft stop instead of hard impulse
      if (!stop_active_) beginSoftStop("AUTOTEST_STOP");

      Serial.println("OK AUTOTEST STOP");
      return;
    }

    if (sub == "core") {
      at_mode_ = 1;
      at_seq_active_ = false;
      at_seq_phase_ = 0;
      startAutotestCoreRun();
      Serial.println("OK AUTOTEST CORE");
      return;
    }

    if (sub == "core2") {
      at_mode_ = 2;
      long gap = 90;
      if (n >= 3) gap = parseLongSafe(tok[2], 90);
      if (gap < 0) gap = 0;
      if (gap > 600) gap = 600;
      at_gap_s_ = (uint16_t)gap;

      at_seq_active_ = true;
      at_seq_phase_ = 1;
      at_phase_t0_ms_ = (uint32_t)millis();

      startAutotestCoreRun();
      Serial.print("OK AUTOTEST CORE2 gap_s="); Serial.println(at_gap_s_);
      return;
    }

    Serial.println("ERR autotest <core|core2|stop> [gap_s]");
    return;
  }

  if (cmd == "throttle") {
    if (n < 2) { Serial.println("ERR throttle <pct>"); return; }
    float pct = parseFloatSafe(tok[1], NAN);
    if (isnan(pct) || !esc_) { Serial.println("ERR throttle"); return; }
    if (!armed_) { Serial.println("ERR NOT_ARMED (use start)"); return; }
    stop_active_ = false; // cancel any pending soft stop
    esc_->setTargetThrottlePct(pct, 0.5f);
    Serial.println("OK THROTTLE");
    return;
  }

  if (cmd == "i2cscan") {
    Serial.println("I2CSCAN:");
    byte count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      uint8_t err = Wire.endTransmission();
      if (err == 0) { Serial.print("  0x"); Serial.println(addr, HEX); count++; }
    }
    Serial.print("FOUND="); Serial.println(count);
    return;
  }

  Serial.println("ERR unknown_cmd");
}

void CLI::printStatus() {
  Serial.println();
  Serial.println("================= STATUS =================");

  Serial.println("SYSTEM");
  Serial.print("  Armed:        "); Serial.println(armed_ ? "YES" : "NO");
  Serial.print("  CSV logging:  "); Serial.println(csv_on_ ? "ON" : "OFF");
  Serial.print("  Notes:        "); Serial.println(notes_);

  Serial.println();
  Serial.println("ESC / CONTROL");
  if (esc_) {
    Serial.print("  Throttle:     ");
    printFinite(esc_->currentThrottlePct(), 2, " %  (target ");
    printFinite(esc_->targetThrottlePct(), 2, " %)\n");

    Serial.print("  Failsafe:     "); Serial.println(esc_->isFailsafe() ? "YES" : "NO");
    Serial.print("  Reason:       "); Serial.println(esc_->failsafeReason());
  }
  Serial.print("  eRPM / RPM:   "); Serial.print(st_erpm_); Serial.print(" / "); Serial.println(st_rpm_);
  Serial.print("  BDShot err:   "); printFinite(st_bdshot_err_pct_, 1, " %\n");

  Serial.println();
  Serial.println("POWER (INA226)");
  Serial.print("  VBAT:         "); printFinite(st_vbus_V_, 3, " V\n");
  Serial.print("  Current:      "); printFinite(st_i_A_, 6, " A\n");
  Serial.print("  Power:        "); printFinite(st_p_W_, 6, " W\n");

  Serial.println();
  Serial.println("THRUST (HX711)");
  Serial.print("  Raw:          "); Serial.println(st_hx_raw_);
  Serial.print("  Offset:       "); Serial.println(st_hx_offset_);
  Serial.print("  Scale:        "); printFinite(st_hx_scale_, 6, " counts/g\n");
  Serial.print("  Cal valid:    "); Serial.println(st_hx_cal_valid_ ? "YES" : "NO");
  Serial.print("  Inverted:     "); Serial.println(st_hx_inverted_ ? "YES" : "NO");
  Serial.print("  Noise p2p:    ");
  if (st_hx_noise_pp_ >= 0) Serial.println(st_hx_noise_pp_);
  else Serial.println("-");

  Serial.print("  Thrust:       ");
  printFinite(st_thrust_g_, 2, " g   (");
  printFinite(st_thrust_N_, 3, " N)\n");

  Serial.println();
  Serial.println("AUTOTEST");
  if (at_ && at_->active()) {
    Serial.println("  Active:       YES");
    Serial.print("  Step ID:      "); Serial.println(at_->stepId());
    Serial.print("  Steady:       "); Serial.println(at_->isSteady() ? "YES" : "NO");
  } else {
    Serial.println("  Active:       NO");
    Serial.println("  Step ID:      -");
    Serial.println("  Steady:       -");
  }

  Serial.println("=========================================");
  Serial.println();
}

// CORE profile (single run) + auto LOG markers for PC rotation
void CLI::startAutotestCoreRun() {
  if (!at_ || !esc_) return;

  static const float steps[] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 60, 0 };
  static const float durs[]  = { 5, 20, 20, 20, 20, 20, 20, 30, 30, 30,  30, 20, 10 };
  static const int N = (int)(sizeof(steps) / sizeof(steps[0]));
  static const float RAMP_S = 3.0f;

  stop_active_ = false; // cancel stop if any
  csv_on_ = true;
  Serial.println("OK LOG 1");

  at_->startProgram(steps, durs, N, RAMP_S);
}

void CLI::serviceAutotestSequence() {
  if (!at_ || !esc_) return;

  // single run => auto stop log when finished
  if (at_mode_ == 1) {
    if (!at_->active()) {
      if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
      Serial.println("OK AUTOTEST DONE");
      at_mode_ = 0;
    }
    return;
  }

  // two runs with a gap
  if (at_mode_ != 2 || !at_seq_active_) return;

  const uint32_t now = (uint32_t)millis();
  const uint32_t age_ms = (uint32_t)(now - at_phase_t0_ms_);

  if (at_seq_phase_ == 1) {
    if (!at_->active()) {
      if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
      Serial.println("OK AUTOTEST GAP");
      at_seq_phase_ = 2;
      at_phase_t0_ms_ = now;
    }
    return;
  }

  if (at_seq_phase_ == 2) {
    if (age_ms >= (uint32_t)at_gap_s_ * 1000u) {
      startAutotestCoreRun();
      Serial.println("OK AUTOTEST RUN2");
      at_seq_phase_ = 3;
      at_phase_t0_ms_ = now;
    }
    return;
  }

  if (at_seq_phase_ == 3) {
    if (!at_->active()) {
      if (csv_on_) { csv_on_ = false; Serial.println("OK LOG 0"); }
      Serial.println("OK AUTOTEST DONE");
      at_seq_active_ = false;
      at_seq_phase_ = 0;
      at_mode_ = 0;
    }
    return;
  }
}
