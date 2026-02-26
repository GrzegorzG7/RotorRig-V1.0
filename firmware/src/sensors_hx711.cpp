#include "sensors_hx711.h"
#include <math.h>

SensorsHx711::SensorsHx711() {
  for (uint8_t i = 0; i < WIN_MAX; i++) window_[i] = 0;
}

bool SensorsHx711::begin(uint8_t dout_gpio, uint8_t sck_gpio) {
  storage_.begin();

  static HX711_ADC lc(dout_gpio, sck_gpio);
  lc_ = &lc;

  lc_->begin();
  lc_->start(2000, false);

  // load calibration if present
  CalData tmp;
  if (storage_.load(tmp)) {
    cal_ = tmp;
    applyCalToLibrary_();
  } else {
    cal_ = CalData{};
    // keep library in harmless defaults
    lc_->setCalFactor(1.0f);
    lc_->setTareOffset(0);
  }

  // prime a bit
  const uint32_t t0 = millis();
  while (millis() - t0 < 200) {
    lc_->update();
    yield();
  }

  sample_count_ = 0;
  last_raw_counts_ = 0;
  tare_busy_ = false;
  tare_done_flag_ = false;
  cal_busy_ = false;
  cal_done_flag_ = false;

  windowReset();
  return true;
}

void SensorsHx711::applyCalToLibrary_() {
  if (!lc_ || !calValid()) return;
  lc_->setTareOffset(cal_.offset);
  const float cf = cal_.invert ? -cal_.scale : cal_.scale; // signed for library
  lc_->setCalFactor(cf);
}

void SensorsHx711::tickFast() {
  if (!lc_) return;

  if (lc_->update()) {
    // getData() returns (raw - offset)/calFactor
    const float val = lc_->getData();

    // reconstruct raw counts regardless of calibration state
    // if cal valid -> raw = offset + val*calFactor
    // else (cal invalid) library calFactor=1, offset=0 -> raw ~= val
    float cf = 1.0f;
    int32_t off = 0;

    if (calValid()) {
      cf = cal_.invert ? -cal_.scale : cal_.scale;
      off = cal_.offset;
    } else {
      // try to use whatever library currently has (defaults at boot are 1 and 0)
      cf = lc_->getCalFactor();
      off = (int32_t)lc_->getTareOffset();
      if (!isfinite(cf) || cf == 0.0f) cf = 1.0f;
    }

    if (isfinite(val)) {
      last_raw_counts_ = (int32_t)lrintf((float)off + val * cf);
    }

    sample_count_++;
  }

  // tare completion
  if (tare_busy_ && lc_->getTareStatus()) {
    cal_.offset = (int32_t)lc_->getTareOffset();
    tare_busy_ = false;
    tare_done_flag_ = true;

    // after tare, reset window so noise/mean restart cleanly
    windowReset();
  }
}

void SensorsHx711::tareTrimStart(uint16_t /*samples*/, uint8_t /*trim_pct*/) {
  if (!lc_) return;
  tare_busy_ = true;
  tare_done_flag_ = false;
  lc_->tareNoDelay();
}

bool SensorsHx711::tareTrimDoneConsume() {
  if (tare_done_flag_) {
    tare_done_flag_ = false;
    return true;
  }
  return false;
}

bool SensorsHx711::calibrateWithMass(float mass_g) {
  if (!lc_ || !(mass_g > 0.0f) || !isfinite(mass_g)) return false;

  if (!lc_->refreshDataSet()) return false;

  float newCal = lc_->getNewCalibration(mass_g);
  if (!isfinite(newCal) || newCal == 0.0f) return false;

  cal_.offset = (int32_t)lc_->getTareOffset();
  cal_.invert = (newCal < 0.0f);
  cal_.scale  = fabsf(newCal);
  cal_.valid  = true;

  applyCalToLibrary_();

  // after cal, reset window too
  windowReset();
  return true;
}

void SensorsHx711::calTrimStart(float mass_g, uint16_t /*samples*/, uint8_t /*trim_pct*/) {
  cal_busy_ = true;
  cal_done_flag_ = false;

  bool ok = calibrateWithMass(mass_g);

  cal_busy_ = false;
  cal_done_flag_ = ok;
}

bool SensorsHx711::calTrimDoneConsume() {
  if (cal_done_flag_) {
    cal_done_flag_ = false;
    return true;
  }
  return false;
}

float SensorsHx711::rawToGrams(int32_t raw) const {
  if (!calValid()) return NAN;
  float delta = (float)(raw - cal_.offset);
  if (cal_.invert) delta = -delta;
  return delta / cal_.scale;
}

HxSample SensorsHx711::convertRawToSample(int32_t raw) const {
  HxSample s;
  if (!calValid()) return s;
  const float g = rawToGrams(raw);
  if (!isfinite(g)) return s;
  s.valid = true;
  s.thrust_g = g;
  s.thrust_N = g * 0.00980665f;
  return s;
}

void SensorsHx711::windowReset() {
  window_head_ = 0;
  window_count_ = 0;
}

void SensorsHx711::windowPush(int32_t raw) {
  window_[window_head_] = raw;
  window_head_ = (uint8_t)((window_head_ + 1) % WIN_MAX);
  if (window_count_ < WIN_MAX) window_count_++;
}

void SensorsHx711::copyWindowToLinear_(int32_t* out, uint8_t& n) const {
  n = window_count_;
  if (n == 0) return;

  // oldest index = head - count (mod WIN_MAX)
  int start = (int)window_head_ - (int)window_count_;
  while (start < 0) start += WIN_MAX;

  for (uint8_t i = 0; i < n; i++) {
    int idx = start + i;
    if (idx >= WIN_MAX) idx -= WIN_MAX;
    out[i] = window_[idx];
  }
}

int32_t SensorsHx711::windowTrimmedMean(uint8_t trim_pct) const {
  if (window_count_ == 0) return last_raw_counts_;

  int32_t tmp[WIN_MAX];
  uint8_t n = 0;
  copyWindowToLinear_(tmp, n);
  if (n == 0) return last_raw_counts_;

  // insertion sort
  for (uint8_t i = 1; i < n; i++) {
    int32_t key = tmp[i];
    int j = (int)i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = key;
  }

  uint8_t k = (uint8_t)((uint16_t)n * (uint16_t)trim_pct / 100U);
  if (k * 2 >= n) k = 0;

  const uint8_t a = k;
  const uint8_t b = (uint8_t)(n - k);
  if (b <= a) return tmp[n / 2];

  int64_t sum = 0;
  uint32_t used = 0;
  for (uint8_t i = a; i < b; i++) { sum += tmp[i]; used++; }
  return (used > 0) ? (int32_t)(sum / (int64_t)used) : tmp[n / 2];
}

HxNoise SensorsHx711::computeNoiseFromWindow_() const {
  HxNoise ns{};
  if (window_count_ < 4) return ns;

  int32_t tmp[WIN_MAX];
  uint8_t n = 0;
  copyWindowToLinear_(tmp, n);
  if (n < 4) return ns;

  int32_t mn = tmp[0], mx = tmp[0];
  double sum = 0.0;
  for (uint8_t i = 0; i < n; i++) {
    int32_t v = tmp[i];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    sum += (double)v;
  }
  double mean = sum / (double)n;

  double acc = 0.0;
  for (uint8_t i = 0; i < n; i++) {
    double d = (double)tmp[i] - mean;
    acc += d * d;
  }

  ns.valid = true;
  ns.raw_pp = (int32_t)(mx - mn);
  ns.std_counts = (n > 1) ? (float)sqrt(acc / (double)(n - 1)) : 0.0f;

  if (calValid()) {
    float p2p_g = (float)ns.raw_pp / cal_.scale;
    float std_g = ns.std_counts / cal_.scale;
    ns.stable = (p2p_g <= 2.0f) || (std_g <= 0.5f);
  } else {
    ns.stable = (ns.raw_pp < 1500);
  }

  return ns;
}

HxNoise SensorsHx711::windowNoise() const {
  return computeNoiseFromWindow_();
}

bool SensorsHx711::saveCal() {
  if (!calValid()) return false;
  return storage_.save(cal_);
}

bool SensorsHx711::loadCal() {
  CalData tmp;
  if (!storage_.load(tmp)) return false;
  cal_ = tmp;
  applyCalToLibrary_();
  windowReset();
  return true;
}

void SensorsHx711::resetCal() {
  cal_ = CalData{};
  storage_.reset();
  if (lc_) {
    lc_->setCalFactor(1.0f);
    lc_->setTareOffset(0);
  }
  windowReset();
}
