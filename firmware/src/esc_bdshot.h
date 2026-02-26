#pragma once
#include <Arduino.h>

struct EscTelemetry {
  bool rpm_valid = false;
  uint32_t erpm = 0;
  uint32_t rpm = 0;
  float bdshot_err_pct = NAN;
};

class EscBdshot {
public:
  bool begin(uint8_t pin, uint16_t dshot_speed);
  void setPolePairs(uint8_t pp) { pole_pairs_ = (pp == 0 ? 1 : pp); }
  uint8_t polePairs() const { return pole_pairs_; }

  void setTargetThrottlePct(float pct, float ramp_s);
  void stopNow();

  // must be called fast (main loop)
  void tickFast();

  // called at log rate
  EscTelemetry getTelemetry();

  float currentThrottlePct() const { return current_throttle_pct_; }
  float targetThrottlePct() const { return target_throttle_pct_; }

  bool isFailsafe() const { return failsafe_; }
  const char* failsafeReason() const { return failsafe_reason_; }

  void clearFailsafe();

private:
  void applyThrottleInternal(float pct);
  uint16_t pctToDshot(float pct) const;

private:
  uint8_t pin_ = 255;
  uint16_t speed_ = 0;

  void* esc_ = nullptr;

  uint8_t pole_pairs_ = 7;

  float current_throttle_pct_ = 0.0f;
  float target_throttle_pct_ = 0.0f;

  float ramp_rate_pct_per_s_ = 9999.0f;
  uint32_t last_ramp_ms_ = 0;

  uint64_t last_send_us_ = 0;

  // telemetry cache
  uint32_t last_rpm_update_ms_ = 0;
  uint32_t last_erpm_cached_ = 0;
  bool telemetry_seen_ = false;

  // failsafe
  bool failsafe_ = false;
  const char* failsafe_reason_ = "OK";
};
