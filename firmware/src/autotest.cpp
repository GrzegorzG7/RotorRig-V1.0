#include "autotest.h"
#include <Arduino.h>

static inline uint32_t ms_now() { return (uint32_t)millis(); }
static inline uint32_t ms_age(uint32_t t0) { return (uint32_t)(ms_now() - t0); } // wrap-safe

void AutoTest::start(const float* steps, int n, float step_time_s, float ramp_s) {
  st_ = AutoTestState{};
  st_.active = true;
  st_.step_time_s = step_time_s;
  st_.ramp_s = ramp_s;
  st_.step_time_list_en = false;

  if (n > AutoTestState::MAX_STEPS) n = AutoTestState::MAX_STEPS;
  st_.count = n;
  for (int i = 0; i < n; i++) st_.steps_pct[i] = steps[i];

  st_.step_id = 0;
  st_.step_start_ms = ms_now();
  st_.steady = false;
}

void AutoTest::startProgram(const float* steps, const float* step_time_s_list, int n, float ramp_s) {
  st_ = AutoTestState{};
  st_.active = true;
  st_.ramp_s = ramp_s;

  if (n > AutoTestState::MAX_STEPS) n = AutoTestState::MAX_STEPS;
  st_.count = n;
  for (int i = 0; i < n; i++) {
    st_.steps_pct[i] = steps[i];
    st_.step_time_s_list[i] = step_time_s_list ? step_time_s_list[i] : 0.0f;
  }
  st_.step_time_list_en = (step_time_s_list != nullptr);

  st_.step_id = 0;
  st_.step_start_ms = ms_now();
  st_.steady = false;
}

float AutoTest::stepTimeS() const {
  if (!st_.active || st_.step_id < 0 || st_.step_id >= st_.count) return NAN;
  if (st_.step_time_list_en) return st_.step_time_s_list[st_.step_id];
  return st_.step_time_s;
}

void AutoTest::stop() {
  st_.active = false;
  st_.step_id = -1;
}

void AutoTest::tick(EscBdshot& esc) {
  if (!st_.active) return;

  const uint32_t elapsed = ms_age(st_.step_start_ms);
  const float step_s = st_.step_time_list_en ? st_.step_time_s_list[st_.step_id] : st_.step_time_s;
  const float step_ms = step_s * 1000.0f;

  // steady after ramp
  st_.steady = (elapsed >= (uint32_t)(st_.ramp_s * 1000.0f));

  // apply target throttle for current step
  const float pct = st_.steps_pct[st_.step_id];
  esc.setTargetThrottlePct(pct, st_.ramp_s);

  if (elapsed >= (uint32_t)step_ms) {
    st_.step_id++;
    if (st_.step_id >= st_.count) {
      st_.active = false;
      st_.step_id = -1;
      esc.setTargetThrottlePct(0.0f, st_.ramp_s);
      return;
    }
    st_.step_start_ms = ms_now();
  }
}
