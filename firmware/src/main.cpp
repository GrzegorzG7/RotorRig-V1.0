#include <Arduino.h>

#include "cfg.h"
#include "frame.h"
#include "meta.h"
#include "csv.h"

#include "esc_bdshot.h"
#include "sensors_hx711.h"
#include "sensors_ina226.h"
#include "cli.h"
#include "autotest.h"

static EscBdshot esc;
static SensorsHx711 hx;
static SensorsIna226 ina;
static CLI cli;
static AutoTest autotest;
static Meta meta;

static uint32_t last_log_ms = 0;

static inline uint32_t now_ms() { return millis(); }
static inline uint32_t ms_since(uint32_t t0) { return (uint32_t)(millis() - t0); }

// === Hardware ===
static constexpr uint8_t HX_DOUT_GPIO = 6;
static constexpr uint8_t HX_SCK_GPIO  = 7;
static constexpr uint8_t ESC_GPIO     = 2;
static constexpr uint8_t INA226_I2C_ADDR = 0x40;

static uint32_t last_hx_sample_count = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  hx.begin(HX_DOUT_GPIO, HX_SCK_GPIO);
  ina.begin(INA226_I2C_ADDR, SHUNT_OHMS, INA_EXPECTED_MAX_CURRENT_A);

  esc.begin(ESC_GPIO, DSHOT_SPEED);
  esc.setPolePairs(meta.pole_pairs);

  cli.begin();
  cli.bind(&esc, &hx, &ina, &meta, &autotest);

  last_log_ms = now_ms();
}

void loop() {
  // 1) ESC MUST tick fast (send throttle + pull telemetry)
  esc.tickFast();

  // 2) HX tick fast
  hx.tickFast();

  // push to rolling HX window only when NEW sample arrives
  const uint32_t sc = hx.sampleCount();
  if (sc != last_hx_sample_count) {
    last_hx_sample_count = sc;
    hx.windowPush(hx.lastRaw());
  }

  // 3) Other periodic logic
  autotest.tick(esc);
  cli.tick();

  // 4) Log/status update at LOG_PERIOD_MS
  const uint32_t now = now_ms();
  if (ms_since(last_log_ms) >= LOG_PERIOD_MS) {
    last_log_ms = now;

    Frame f;
    f.t_ms = now;

    // Autotest context (useful for post-processing)
    if (autotest.active()) {
      f.step_id = autotest.stepId();
      f.step_time_s = autotest.stepTimeS();
      f.is_steady = autotest.isSteady() ? 1 : 0;
    }

    // ESC telemetry
    auto tel = esc.getTelemetry();
    f.erpm = tel.erpm;
    f.rpm = tel.rpm;
    f.bdshot_err_pct = tel.bdshot_err_pct;

    // INA
    InaSample is = ina.read();
    f.v_bus_V = is.v_bus_V;
    f.i_A = is.i_A;
    f.p_in_W = is.p_W;

    // HX noise + thrust (ROLLING WINDOW, no reset here)
    auto nz = hx.windowNoise();
    f.hx_noise_pp = nz.valid ? nz.raw_pp : -1;

    int32_t raw_for_thrust = hx.lastRaw();
    if (hx.windowHasEnough(4)) {
      raw_for_thrust = hx.windowTrimmedMean(20); // 20% trim
    }

    HxSample hs = hx.convertRawToSample(raw_for_thrust);
    if (hs.valid && isfinite(hs.thrust_g) && isfinite(hs.thrust_N)) {
      f.thrust_g = hs.thrust_g;
      f.thrust_N = hs.thrust_N;
    } else {
      f.thrust_g = NAN;
      f.thrust_N = NAN;
    }

    // efficiencies
    if (isfinite(f.thrust_g) && isfinite(f.p_in_W) && f.p_in_W > 0.1f) f.eff_g_per_W = f.thrust_g / f.p_in_W;
    if (isfinite(f.thrust_N) && isfinite(f.p_in_W) && f.p_in_W > 0.1f) f.eff_N_per_W = f.thrust_N / f.p_in_W;
    if (isfinite(f.thrust_g) && isfinite(f.i_A) && fabsf(f.i_A) > 0.01f) f.eff_g_per_A = f.thrust_g / f.i_A;

    f.throttle_pct = esc.currentThrottlePct();

    // update CLI live snapshot for STATUS
    cli.setLive(
      f.thrust_g, f.thrust_N,
      f.v_bus_V, f.i_A, f.p_in_W,
      f.erpm, f.rpm, f.bdshot_err_pct,
      hx.lastRaw(), hx.offset(), hx.scaleCountsPerG(),
      hx.calValid(), hx.inverted(),
      f.hx_noise_pp
    );

    if (cli.csvOn()) {
      printCsvFrame(f, meta, esc, cli.notes());
    }
  }
}
