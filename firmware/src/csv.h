#pragma once
#include <Arduino.h>
#include "frame.h"
#include "meta.h"

class EscBdshot;

// Print CSV line in required 24-column format (no header)
void printCsvFrame(const Frame& f, const Meta& meta, const EscBdshot& esc, const String& notes);
