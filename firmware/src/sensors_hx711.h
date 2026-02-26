#pragma once

#define SENSORS_HX711_HEADER_VERSION 20260120
#pragma message("USING sensors_hx711.h v20260120b => " __FILE__)

#include <Arduino.h>
#include <math.h>

#include <HX711_ADC.h>
#include "storage.h"

// Sample in physical units (used by main.cpp / STATUS)
struct HxSample {
  bool  valid = false;
  float thrust_g = NAN;
  float thrust_N = NAN;
};

// Noise info (used by main.cpp -> hx_noise_pp)
// IMPORTANT: sensors_hx711.cpp expects std_counts and stable.
struct HxNoise {
  bool    valid = false;
  int32_t raw_pp = -1;        // peak-to-peak in raw counts
  float   std_counts = 0.0f;  // standard deviation in counts
  bool    stable = false;     // stability heuristic
};

class SensorsHx711 {
public:
  SensorsHx711();

  bool begin(uint8_t dout_gpio, uint8_t sck_gpio);
  void tickFast();

  // TARE / CAL (names match sensors_hx711.cpp)
  void tareTrimStart(uint16_t samples, uint8_t trim_pct);
  bool tareTrimDoneConsume();

  bool calibrateWithMass(float mass_g);
  void calTrimStart(float mass_g, uint16_t samples, uint8_t trim_pct);
  bool calTrimDoneConsume();

  // NVM calibration
  bool saveCal();
  bool loadCal();
  void resetCal();  // NOTE: void (matches .cpp)

  // Runtime accessors (used in main.cpp/CLI status)
  uint32_t sampleCount() const { return sample_count_; }
  int32_t  lastRaw() const { return last_raw_counts_; }

  bool   calValid() const { return cal_.valid; }
  bool   inverted() const { return cal_.invert; }
  int32_t offset() const { return cal_.offset; }
  float  scaleCountsPerG() const { return cal_.scale; }

  // Raw -> grams (keeps sign; invert handled only by cal_.invert; NO abs() in runtime)
  float rawToGrams(int32_t raw) const;

  // Raw -> sample (grams + newtons)
  HxSample convertRawToSample(int32_t raw) const;

  // Windowing (80 SPS -> 10 Hz log)
  void windowReset();
  void windowPush(int32_t raw);

  bool windowHasEnough(uint8_t n) const { return window_count_ >= n; }
  int32_t windowTrimmedMean(uint8_t trim_pct) const;
  HxNoise windowNoise() const;

private:
  // Internal helpers (exist in sensors_hx711.cpp)
  void applyCalToLibrary_();
  void copyWindowToLinear_(int32_t* out, uint8_t& n) const;
  HxNoise computeNoiseFromWindow_() const;

private:
  HX711_ADC* lc_ = nullptr;

  CalStorage storage_;
  CalData    cal_;

  uint32_t sample_count_ = 0;
  int32_t  last_raw_counts_ = 0;

  bool tare_busy_ = false;
  bool tare_done_flag_ = false;

  bool cal_busy_ = false;
  bool cal_done_flag_ = false;

  // Smaller window = lower lag (still respects "add only on new sample")
  static constexpr uint8_t WIN_MAX = 12;  // was 32
  int32_t window_[WIN_MAX];
  uint8_t window_head_  = 0;  // next write index
  uint8_t window_count_ = 0;  // valid samples in ring
};
