#pragma once
#include <Arduino.h>

static inline uint32_t now_ms() { return (uint32_t)millis(); }
static inline uint32_t ms_since(uint32_t t0) { return (uint32_t)(now_ms() - t0); }

static inline uint64_t now_us() { return (uint64_t)micros(); }
static inline uint64_t us_since(uint64_t t0) { return (uint64_t)(now_us() - t0); }
