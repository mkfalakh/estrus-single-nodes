#pragma once

#include <Arduino.h>

// bool loadPartitionBaseline(uint8_t partition,
//                            float &baselineRate,
//                            uint32_t &baselineSamples);

bool loadGlobalBaseline(
  float &baselineRate,
  uint32_t &baselineSamples);

extern uint32_t csvRowsWritten;

void invalidateBaselineCache();
