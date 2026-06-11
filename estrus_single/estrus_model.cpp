#include "estrus_model.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "storage_stats.h"
#include "logger.h"

#define MAX_PARTITIONS 24
#define MIN_BASELINE_SAMPLES 100

static PartitionStats todayStats[MAX_PARTITIONS];
static uint32_t todayStanding = 0;
static uint32_t todayTotal = 0;
static uint8_t lastPartition = 255;

// ======================================
// RESET STANDING
// ======================================

void resetTodayStats() {

  todayStanding = 0;

  todayTotal = 0;

  logToFile(
    "📊 Today stats reset");
}


// ======================================
// ACCESSOR
// ======================================

PartitionStats *getTodayStats() {

  return todayStats;
}


// ======================================
// UPDATE CURRENT PARTITION
// ======================================

// MENGELOLA PERGANTIAN PARTITION
void checkPartitionTransition() {

  if (!SYS.rtc_ok) {
    return;
  }

  if (sysConfig.partition_hours == 0) {
    return;
  }

  DateTime now = getNow();

  uint8_t currentPartition =
    now.hour() / sysConfig.partition_hours;

  // pertama kali boot
  if (lastPartition == 255) {

    lastPartition = currentPartition;

    return;
  }

  // partition berubah
  if (currentPartition != lastPartition) {

    resetAlarmAcknowledgement();

    invalidateBaselineCache();

    logToFile(
      "🔔 Partition changed: %u -> %u",
      lastPartition,
      currentPartition);

    lastPartition = currentPartition;
  }
}


// MENGHITUNG STANDING STATS
void updatePartitionStats(bool standing) {

  if (standing) {

    todayStanding++;
  }

  todayTotal++;
}


// ======================================
// PLACEHOLDER BASELINE
// nanti dipindah ke storage_stats.cpp
// ======================================

static bool loadBaseline(

  uint8_t partition,

  float &baselineRate,
  uint32_t &baselineSamples) {

  baselineRate = 0.50f;

  baselineSamples = 500;

  return true;
}


// ======================================
// MAIN EVALUATION
// ======================================

EstrusResult evaluateEstrus() {

  EstrusResult r;

  memset(
    &r,
    0,
    sizeof(r));

  uint8_t p =
    getPartitionIndex();

  r.partition = p;

  if (p >= MAX_PARTITIONS)
    return r;

  PartitionStats &s =
    todayStats[p];

  if (s.total == 0)
    return r;

  // -----------------------------
  // current rate
  // -----------------------------

  r.current_rate =
    (float)s.standing / (float)s.total;

  // -----------------------------
  // baseline
  // -----------------------------

  float baselineRate;

  uint32_t baselineSamples;

  if (
    !loadBaseline(
      p,
      baselineRate,
      baselineSamples)) {

    return r;
  }

  r.baseline_rate =
    baselineRate;

  r.baseline_samples =
    baselineSamples;

  // -----------------------------
  // baseline guard
  // -----------------------------

  if (
    baselineSamples < MIN_BASELINE_SAMPLES) {

    r.valid = false;

    return r;
  }

  // -----------------------------
  // avoid divide by zero
  // -----------------------------

  if (
    baselineRate <= 0.0001f) {

    r.valid = false;

    return r;
  }

  // -----------------------------
  // deviation %
  // -----------------------------

  r.deviation_pct =
    (r.current_rate - baselineRate)
    / baselineRate
    * 100.0f;

  // -----------------------------
  // estrus decision
  // -----------------------------

  r.estrus =
    (r.deviation_pct > sysConfig.estrus_threshold_pct);

  r.valid = true;

  return r;
}
