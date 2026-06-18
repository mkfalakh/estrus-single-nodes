#pragma once

#include <Arduino.h>

// struct PartitionStats {

//   uint32_t standing;
//   uint32_t total;
// };

struct GlobalStats {

  uint32_t standing;
  uint32_t total;
};

struct EstrusResult {

  bool valid;
  bool estrus;

  float current_rate;
  float baseline_rate;
  float deviation_pct;

  // uint8_t partition;

  uint32_t baseline_samples;
};

// void resetRuntimePartitionStats(uint8_t partition);
// void updatePartitionStats(bool standing);
// PartitionStats *getTodayStats();
void checkTimeTransitions();
void updateGlobalStats(bool standing);
void resetGlobalStats();
EstrusResult evaluateEstrus();

