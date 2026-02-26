#pragma once
#include <Arduino.h>

class EscBdshot;
class SensorsHx711;
class SensorsIna226;
struct Meta;
class AutoTest;

class CLI {
public:
  void begin();
  void bind(EscBdshot* esc, SensorsHx711* hx, SensorsIna226* ina, Meta* meta, AutoTest* at);

  void tick();
  void handleLine(const String& line);

  // runtime control
  bool csvOn() const { return csv_on_; }
  bool armed() const { return armed_; }
  const String& notes() const { return notes_; }   // public getter

  // live snapshot for status
  void setLive(float thrust_g, float thrust_N,
               float vbus_V, float i_A, float p_W,
               uint32_t erpm, uint32_t rpm, float bdshot_err_pct,
               int32_t hx_raw, int32_t hx_offset, float hx_scale,
               bool hx_cal_valid, bool hx_inverted,
               int32_t hx_noise_pp);

private:
  void printStatus();

  // autotest helpers (from your previous version)
  void serviceAutotestSequence();
  void startAutotestCoreRun();

  // soft-stop
  void beginSoftStop(const char* reason_tag);
  void serviceSoftStop();

private:
  String buf_;

  // bindings
  EscBdshot* esc_ = nullptr;
  SensorsHx711* hx_ = nullptr;
  SensorsIna226* ina_ = nullptr;
  Meta* meta_ = nullptr;
  AutoTest* at_ = nullptr;

  // state
  bool armed_ = false;
  bool csv_on_ = false;
  String notes_ = "OK";

  // autotest sequence (CORE2)
  uint8_t at_mode_ = 0;          // 0=idle, 1=core (single), 2=core2 (two runs)
  bool at_seq_active_ = false;
  uint8_t at_seq_phase_ = 0;      // 0=idle, 1=run1, 2=gap, 3=run2
  uint32_t at_phase_t0_ms_ = 0;
  uint16_t at_gap_s_ = 90;

  // === SOFT STOP settings/state ===
  bool stop_active_ = false;
  uint32_t stop_t0_ms_ = 0;
  float stop_ramp_s_ = 2.5f;          // default compromise
  float stop_cut_pct_ = 0.8f;         // when below this, we disarm (stopNow)
  uint32_t stop_timeout_ms_ = 6000;   // failsafe timeout for stopping
  const char* stop_reason_ = "STOP";

  // live snapshot values
  float st_thrust_g_ = NAN;
  float st_thrust_N_ = NAN;

  float st_vbus_V_ = NAN;
  float st_i_A_ = NAN;
  float st_p_W_ = NAN;

  uint32_t st_erpm_ = 0;
  uint32_t st_rpm_ = 0;
  float st_bdshot_err_pct_ = 100.0f;

  int32_t st_hx_raw_ = 0;
  int32_t st_hx_offset_ = 0;
  float st_hx_scale_ = 1.0f;
  bool st_hx_cal_valid_ = false;
  bool st_hx_inverted_ = false;
  int32_t st_hx_noise_pp_ = -1;
};
