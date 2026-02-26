#include "esc_bdshot.h"
#include "cfg.h"

#include <Arduino.h>
#include <math.h>
#include <PIO_DShot.h>  // pico-bidir-dshot

static inline float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline uint32_t ms_now() { return (uint32_t)millis(); }
static inline uint64_t us_now() { return (uint64_t)micros(); }

// After this age (at low throttle), we'll present RPM=0 to avoid "stale cached RPM" in STATUS/CSV.
static constexpr uint32_t STOPPED_STALE_RPM_MS = 250;

bool EscBdshot::begin(uint8_t pin, uint16_t dshot_speed) {
  pin_ = pin;
  speed_ = dshot_speed;

  auto* e = new BidirDShotX1(pin_, speed_);
  esc_ = (void*)e;

  current_throttle_pct_ = 0.0f;
  target_throttle_pct_  = 0.0f;

  ramp_rate_pct_per_s_ = 9999.0f;
  last_ramp_ms_ = ms_now();
  last_send_us_ = us_now();

  // Start with clean telemetry state
  last_rpm_update_ms_ = ms_now();
  last_erpm_cached_ = 0;
  telemetry_seen_ = false;

  clearFailsafe();
  return true;
}

void EscBdshot::clearFailsafe() {
  // Clear failsafe latch
  failsafe_ = false;
  failsafe_reason_ = "OK";

  // Reset telemetry expectation so RPM_TIMEOUT cannot trigger
  // based on stale timestamps from previous run.
  telemetry_seen_ = false;
  last_erpm_cached_ = 0;
  last_rpm_update_ms_ = ms_now();
}

void EscBdshot::stopNow() {
  // Bring throttle to zero immediately
  target_throttle_pct_ = 0.0f;
  current_throttle_pct_ = 0.0f;
  ramp_rate_pct_per_s_ = 9999.0f;

  // Reset telemetry/failsafe state for next START
  clearFailsafe();

  // One immediate send (loop will continue sending at fixed period)
  applyThrottleInternal(0.0f);
}

void EscBdshot::setTargetThrottlePct(float pct, float ramp_s) {
  pct = clampf(pct, 0.0f, 100.0f);
  target_throttle_pct_ = pct;

  if (ramp_s <= 0.0f) ramp_rate_pct_per_s_ = 9999.0f;
  else ramp_rate_pct_per_s_ = 100.0f / ramp_s;
}

uint16_t EscBdshot::pctToDshot(float pct) const {
  pct = clampf(pct, 0.0f, 100.0f);
  const float v = (pct / 100.0f) * (float)DSHOT_MAX;
  uint16_t out = (uint16_t)(v + 0.5f);
  if (out > DSHOT_MAX) out = DSHOT_MAX;
  return out;
}

void EscBdshot::applyThrottleInternal(float pct) {
  if (!esc_) return;
  auto* e = (BidirDShotX1*)esc_;
  e->sendThrottle(pctToDshot(pct));
}

void EscBdshot::tickFast() {
  if (!esc_) return;

  const uint32_t now_ms = ms_now();

  // If failsafe latched: force throttle to 0 but keep sending at a fixed period
  if (failsafe_) {
    target_throttle_pct_ = 0.0f;
    current_throttle_pct_ = 0.0f;
  } else {
    // 1) ramp current throttle toward target
    const uint32_t dt_ms = (uint32_t)(now_ms - last_ramp_ms_);
    last_ramp_ms_ = now_ms;

    const float dt_s = (float)dt_ms * 0.001f;
    const float max_step = ramp_rate_pct_per_s_ * dt_s;

    const float err = target_throttle_pct_ - current_throttle_pct_;
    if (fabsf(err) <= max_step) current_throttle_pct_ = target_throttle_pct_;
    else current_throttle_pct_ += (err > 0.0f ? max_step : -max_step);
  }

  // 2) send throttle at fixed period, and only then pull telemetry (bounded work)
  const uint64_t now_us = us_now();
  if ((uint32_t)(now_us - last_send_us_) >= ESC_SEND_PERIOD_US) {
    last_send_us_ = now_us;

    applyThrottleInternal(current_throttle_pct_);

    // 3) telemetry pull + cache (only on send)
    uint32_t erpm = 0;
    auto* e = (BidirDShotX1*)esc_;
    e->getTelemetryErpm(&erpm);

    if (erpm > 0) {
      last_erpm_cached_ = erpm;
      telemetry_seen_ = true;
      last_rpm_update_ms_ = now_ms;
    }
  }

  // 4) failsafe only if telemetry was seen in THIS run and throttle is real
  const uint32_t age_ms = (uint32_t)(now_ms - last_rpm_update_ms_);
  if (!failsafe_ && telemetry_seen_ && age_ms > TELEMETRY_TIMEOUT_MS && current_throttle_pct_ > 3.0f) {
    failsafe_ = true;
    failsafe_reason_ = "RPM_TIMEOUT";
  }
}

EscTelemetry EscBdshot::getTelemetry() {
  EscTelemetry t;

  const uint32_t now = ms_now();
  const uint32_t age_ms = (uint32_t)(now - last_rpm_update_ms_);
  const bool low_throttle = (current_throttle_pct_ < 1.0f && target_throttle_pct_ < 1.0f);

  // If we're essentially stopped and telemetry is stale, don't show old cached RPM.
  if (low_throttle && age_ms > STOPPED_STALE_RPM_MS) {
    t.erpm = 0;
    t.rpm_valid = false;
    t.rpm = 0;

    // When stopped / not expecting telemetry, don't scare with 100%.
    t.bdshot_err_pct = NAN;
    return t;
  }

  // Normal path: show cached telemetry
  t.erpm = last_erpm_cached_;
  if (t.erpm > 0 && pole_pairs_ > 0) {
    t.rpm_valid = true;
    t.rpm = (uint32_t)(t.erpm / (uint32_t)pole_pairs_);
  } else {
    t.rpm_valid = false;
    t.rpm = 0;
  }

  // If telemetry was never seen yet, report NaN (unknown) instead of 100%.
  if (!telemetry_seen_) {
    t.bdshot_err_pct = NAN;
  } else {
    t.bdshot_err_pct = (age_ms > TELEMETRY_TIMEOUT_MS) ? 100.0f : 0.0f;
  }

  return t;
}
