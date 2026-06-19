#pragma once

#include <Arduino.h>

extern uint32_t csvRowsWritten;

// Scan all historical CSV files and populate the in-RAM baseline for every
// partition in one pass. Call at boot, on day rollover, and on any config
// change that affects the baseline (partition_hours, retention_days,
// min_baseline_samples).
void triggerBaselineRecompute();

// Read the pre-computed baseline for a partition from RAM. Returns false if
// no data is available (recompute not yet done, or no historical files found).
bool getCachedBaseline(uint8_t partition,
                       float   &baselineRate,
                       uint32_t &baselineSamples);
