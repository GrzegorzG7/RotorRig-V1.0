#pragma once
#include <Arduino.h>

// Metadata for CSV and status.
// Keep defaults "NA"/-1 so CSV remains valid even without setmeta.
struct Meta {
  String test_id   = "NA";
  String motor_id  = "NA";
  int    kv        = -1;
  String prop      = "NA";
  int    battery_s = -1;
  String esc_fw    = "NA";
  uint8_t pole_pairs = 7;
};
