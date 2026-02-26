#pragma once
#include <Arduino.h>

// --- Pins ---
static constexpr uint8_t PIN_DSHOT = 2;     // GP2
static constexpr uint8_t PIN_HX_DOUT = 6;   // GP6
static constexpr uint8_t PIN_HX_SCK  = 7;   // GP7

// --- Serial / logging ---
static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t LOG_PERIOD_MS = 100;     // 10 Hz
static constexpr uint32_t ESC_SEND_PERIOD_US = 1000; // 1 kHz sendThrottle (>=500Hz recommended) :contentReference[oaicite:5]{index=5}

// --- ESC / Motor ---
static constexpr uint16_t DSHOT_SPEED = 300; // DShot300
static constexpr uint16_t DSHOT_MIN = 0;
static constexpr uint16_t DSHOT_MAX = 2000; // library convention
static constexpr uint32_t TELEMETRY_TIMEOUT_MS = 500; // if no RPM updates -> failsafe

// --- INA226 ---
static constexpr uint8_t INA226_ADDR_DEFAULT = 0x40; // change if needed
static constexpr float SHUNT_OHMS = 0.001f;          // 1 mΩ
static constexpr float INA_EXPECTED_MAX_CURRENT_A = 60.0f; // safe default; tweak later

// --- HX711 ---
static constexpr uint8_t HX711_SPS_TARGET = 80; // requirement
static constexpr uint8_t HX_SAMPLES_PER_LOG = 8; // ~80 SPS / 10 Hz

// --- Safety ---
static constexpr uint32_t STARTUP_ARM_ZERO_MS = 400; // send zero a bit at boot
static constexpr float VBAT_PRESENT_THRESHOLD_V = 1.0f;
