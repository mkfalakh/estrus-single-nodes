#pragma once

#include <Arduino.h>

struct EstrusResult {
  bool  valid;      // baseline has enough healthy windows
  bool  estrus;     // z >= z_threshold AND calendar gate passed
  bool  untrusted;  // quality gate failed (dirty or stuck sensor in window)

  float current_rate;   // sensor2 on_frac × 100 for current sliding window
  float baseline_rate;  // median on_frac × 100 from historical baseline
  float z_score;        // (on_frac - median) / normalized_MAD
  float deviation_pct;  // (z / z_threshold) × 100; ≥100 means threshold crossed

  uint16_t baseline_windows;  // healthy windows used to build the baseline
};

// Feed one sensor reading into the sliding detection window.
// Call once per record_interval_sec from the sensor task.
void updateSensor2(bool s2, bool d1, bool d2);

// Reset the sliding window and dirty-window state.
// Call when record_interval_sec changes or on explicit reset.
void resetSlidingWindow();

// Detect day/partition transitions and trigger baseline recompute on day rollover.
void checkTimeTransitions();

// Evaluate estrus from the current sliding window state.
// Returns a zeroed result if the window is not yet full or RTC is unavailable.
EstrusResult evaluateEstrus();

// Debug helpers: current fill count and required size of the sliding window.
uint16_t getSlidingWindowCount();
uint16_t getSlidingWindowSize();

// Prefill the sliding window from the last N clean CSV records at boot.
// Call once in setup() after triggerBaselineRecompute(), before startTasks().
void prefillSlidingWindow();
