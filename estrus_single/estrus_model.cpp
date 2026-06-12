#include "estrus_model.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "storage_stats.h"
#include "logger.h"

#define MAX_PARTITIONS 24

static PartitionStats partitionStats[MAX_PARTITIONS];
static uint8_t lastPartition = 255;
static uint8_t lastDay = 255;

static uint8_t getPartitionIndex() {

  DateTime now = getNow();

  return now.hour() / sysConfig.partition_hours;
}

// ======================================
// RESET STANDING
// ======================================

void resetRuntimePartitionStats(uint8_t partition) {

  // uint8_t partition = getPartitionIndex();

  if (partition >= MAX_PARTITIONS) {
    return;
  }

  partitionStats[partition].standing = 0;
  partitionStats[partition].total = 0;

  logToFile(
    "📊 Partition %u runtime stats reset",
    partition);
}


// ======================================
// ACCESSOR
// ======================================

PartitionStats *getRuntimePartitionStats() {

  return partitionStats;
}


// ======================================
// UPDATE CURRENT PARTITION
// ======================================

// MENGELOLA PERGANTIAN PARTITION
void checkTimeTransitions() {

  if (!SYS.rtc_ok) {
    logToFile("RTC unknown! gagal cek partition!");
    return;
  }

  if (sysConfig.partition_hours == 0) {
    logToFile("Partition tidak boleh 0! gagal cek partition!");
    return;
  }

  DateTime now = getNow();

  uint8_t newDay = now.day();

  uint8_t newPartition =
    now.hour() / sysConfig.partition_hours;

  // pertama kali boot
  if (lastPartition == 255) {

    lastPartition = newPartition;
    lastDay = newDay;

    return;
  }

  // ======================
  // DAY TRANSITION
  // ======================
  if (lastDay != 255 && newDay != lastDay) {

    invalidateBaselineCache();

    resetAlarmAcknowledgement();

    logToFile(
      "📅 New day → baseline cache reset");
  }

  // ======================
  // PARTITION TRANSITION
  // ======================
  if (lastPartition != 255 && newPartition != lastPartition) {

    resetRuntimePartitionStats(newPartition);

    logToFile(
      "🕒 Partition %u → %u",
      lastPartition,
      newPartition);
  }

  lastDay = newDay;
  lastPartition = newPartition;
}


// MENGHITUNG STANDING STATS
void updatePartitionStats(bool standing) {

  uint8_t p = getPartitionIndex();

  if (p >= MAX_PARTITIONS) {
    return;
  }

  if (standing) {
    partitionStats[p].standing++;
  }

  partitionStats[p].total++;
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
    partitionStats[p];

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

  if (!loadPartitionBaseline(
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
    baselineSamples < sysConfig.min_baseline_samples) {

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
    (r.deviation_pct >= sysConfig.estrus_threshold_pct);

  r.valid = true;

  return r;
}
