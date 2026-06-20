#pragma once

#include <Arduino.h>

extern uint32_t csvRowsWritten;

// Scan all historical CSV files and populate the in-RAM baseline for every
// partition in one pass. Call at boot, on day rollover, and on any config
// change that affects the baseline (partition_hours, retention_days,
// min_baseline_windows).
void triggerBaselineRecompute();

// Read the pre-computed baseline for a partition from RAM.
// medianRate    — median on_frac of sensor2_state across healthy historical windows
// madRate       — normalized MAD (MAD × 1.4826), ready for z-score denominator
// nWindows      — number of healthy windows used to build the baseline
// Returns false if no data is available (recompute not done or no files found).
bool getCachedBaseline(uint8_t   partition,
                       float    &medianRate,
                       float    &madRate,
                       uint16_t &nWindows);
