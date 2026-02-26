#pragma once
#include <Arduino.h>
#include "esc_bdshot.h"

struct AutoTestState {
  bool active = false;
  int step_id = -1;
  // Default (uniform) step time. If step_time_list_en==true,
  // then each step has its own duration in step_time_s_list[].
  float step_time_s = 0;
  float ramp_s = 0;
  uint32_t step_start_ms = 0;
  bool steady = false;

  // steps list
  static constexpr int MAX_STEPS = 32;
  int count = 0;
  float steps_pct[MAX_STEPS]{};

  // Optional per-step durations.
  bool step_time_list_en = false;
  float step_time_s_list[MAX_STEPS]{};
};

class AutoTest {
public:
  void start(const float* steps, int n, float step_time_s, float ramp_s);
  void startProgram(const float* steps, const float* step_time_s_list, int n, float ramp_s);
  void stop();

  void tick(EscBdshot& esc);

  bool active() const { return st_.active; }
  int stepId() const { return st_.step_id; }
  float stepTimeS() const;
  bool isSteady() const { return st_.steady; }

private:
  AutoTestState st_;
};
