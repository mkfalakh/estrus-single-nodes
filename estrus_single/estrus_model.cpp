#include "estrus_model.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "storage_stats.h"
#include "logger.h"

// #define MAX_PARTITIONS 24

// static PartitionStats partitionStats[MAX_PARTITIONS];
// static uint8_t lastPartition = 255;
static uint8_t lastDay = 255;

static GlobalStats stats;

// helper
void resetGlobalStats() {

  stats.standing = 0;
  stats.total = 0;

  invalidateBaselineCache();

  logToFile(
    "📊 Global stats reset");
}

// static void resetPartitionRuntime() {

//   memset(
//     partitionStats,
//     0,
//     sizeof(partitionStats));

//   lastPartition = 255;
//   lastDay = 255;

//   invalidateBaselineCache();

//   logToFile(
//     "📊 Partition runtime reset");
// }

// static uint8_t getPartitionIndex() {

//   DateTime now = getNow();

//   return now.hour() / sysConfig.partition_hours;
// }

// ======================================
// RESET STANDING
// ======================================

// void resetRuntimePartitionStats(uint8_t partition) {

//   // uint8_t partition = getPartitionIndex();

//   if (partition >= MAX_PARTITIONS) {
//     return;
//   }

//   partitionStats[partition].standing = 0;
//   partitionStats[partition].total = 0;

//   logToFile(
//     "📊 Partition %u runtime stats reset",
//     partition);
// }


// ======================================
// ACCESSOR
// ======================================

// PartitionStats *getRuntimePartitionStats() {

//   return partitionStats;
// }


// ======================================
// UPDATE CURRENT TRANSITIONS
// ======================================

// MENGELOLA PERGANTIAN WAKTU TRANSISI
void checkTimeTransitions() {

  if (!SYS.rtc_ok) {
    logToFile("RTC invalid! gagal cek transisi");
    return;
  }

  uint8_t today =
    getNow().day();

  if (lastDay == 255) {

    lastDay = today;

    return;
  }

  if (today != lastDay) {

    resetGlobalStats();

    resetAlarmAcknowledgement();

    lastDay = today;

    logToFile(
      "📅 New day");
  }
}

// void checkTimeTransitions() {

//   if (!SYS.rtc_ok) {
//     logToFile("RTC unknown! gagal cek partition!");
//     return;
//   }

//   if (sysConfig.partition_hours == 0) {
//     logToFile("Partition tidak boleh 0! gagal cek partition!");
//     return;
//   }

//   static uint8_t lastPartitionHours = 0;

//   if (lastPartitionHours == 0) {

//     lastPartitionHours =
//       sysConfig.partition_hours;

//   } else if (
//     lastPartitionHours != sysConfig.partition_hours) {

//     logToFile(
//       "📊 Partition config changed %u -> %u",

//       lastPartitionHours,
//       sysConfig.partition_hours);

//     resetPartitionRuntime();

//     lastPartitionHours =
//       sysConfig.partition_hours;

//     return;
//   }

//   DateTime now = getNow();

//   uint8_t newDay = now.day();

//   uint8_t newPartition =
//     now.hour() / sysConfig.partition_hours;

//   // pertama kali boot
//   if (lastPartition == 255) {

//     lastPartition = newPartition;
//     lastDay = newDay;

//     return;
//   }

//   // ======================
//   // DAY TRANSITION
//   // ======================
//   if (lastDay != 255 && newDay != lastDay) {

//     invalidateBaselineCache();

//     resetAlarmAcknowledgement();

//     logToFile(
//       "📅 New day → baseline cache reset");
//   }

//   // ======================
//   // PARTITION TRANSITION
//   // ======================
//   if (lastPartition != 255 && newPartition != lastPartition) {

//     resetRuntimePartitionStats(newPartition);

//     logToFile(
//       "🕒 Partition last: %u → now: %u",
//       lastPartition,
//       newPartition);
//   }

//   lastDay = newDay;
//   lastPartition = newPartition;
// }


// MENGHITUNG STANDING STATS
void updateGlobalStats(bool standing) {

  if (standing) {
    stats.standing++;
  }

  stats.total++;
}

// void updatePartitionStats(bool standing) {

//   static uint32_t dbg = 0;

//   uint8_t p = getPartitionIndex();

//   if (++dbg % 100 == 0) {

//     logToFile(
//       "Partition %u | standing=%lu | total=%lu",

//       p,

//       partitionStats[p].standing,

//       partitionStats[p].total);
//   }

//   if (p >= MAX_PARTITIONS) {
//     return;
//   }

//   if (standing) {
//     partitionStats[p].standing++;
//   }

//   partitionStats[p].total++;
// }


// ======================================
// MAIN EVALUATION
// ======================================
EstrusResult evaluateEstrus() {

  EstrusResult r;

  memset(
    &r,
    0,
    sizeof(r));

  if (stats.total == 0) {
    return r;
  }

  r.current_rate =
    (float)stats.standing / (float)stats.total;

  float baselineRate = 0;
  uint32_t baselineSamples = 0;

  if (!loadGlobalBaseline(
        baselineRate,
        baselineSamples)) {

    logToFile(
      "❌ Baseline load failed");

    return r;
  }

  r.baseline_rate =
    baselineRate;

  r.baseline_samples =
    baselineSamples;

  if (baselineSamples < sysConfig.min_baseline_samples) {

    return r;
  }

  if (baselineRate <= 0.0001f) {

    return r;
  }

  r.deviation_pct =
    ((r.current_rate - baselineRate)
     / baselineRate)
    * 100.0f;

  r.estrus =
    (r.deviation_pct >= sysConfig.estrus_threshold_pct);

  r.valid = true;

  return r;
}


// EstrusResult evaluateEstrus() {

//   EstrusResult r;

//   memset(
//     &r,
//     0,
//     sizeof(r));

//   uint8_t p =
//     getPartitionIndex();

//   logToFile(
//     "ESTRUS CONFIG partition=%u partition_hours=%u retention=%u threshold=%.2f",

//     p,

//     sysConfig.partition_hours,

//     sysConfig.retention_days,

//     sysConfig.estrus_threshold_pct);

//   r.partition = p;

//   if (sysConfig.partition_hours == 0) {

//     logToFile(
//       "❌ partition_hours invalid");

//     return r;
//   }

//   if (p >= MAX_PARTITIONS) {

//     logToFile(
//       "❌ partition overflow p=%u",
//       p);

//     return r;
//   }

//   PartitionStats &s =
//     partitionStats[p];

//   if (s.total == 0)
//     return r;

//   // -----------------------------
//   // current rate
//   // -----------------------------

//   r.current_rate =
//     (float)s.standing / (float)s.total;

//   // -----------------------------
//   // baseline
//   // -----------------------------

//   float baselineRate;

//   uint32_t baselineSamples;

//   if (!loadPartitionBaseline(
//         p,
//         baselineRate,
//         baselineSamples)) {

//     logToFile(
//       "❌ Baseline load failed! p=%u",
//       p);

//     return r;
//   }

//   r.baseline_rate =
//     baselineRate;

//   r.baseline_samples =
//     baselineSamples;

//   // -----------------------------
//   // baseline guard
//   // -----------------------------

//   if (
//     baselineSamples < sysConfig.min_baseline_samples) {

//     r.valid = false;

//     return r;
//   }

//   // -----------------------------
//   // avoid divide by zero
//   // -----------------------------

//   if (
//     baselineRate <= 0.0001f) {

//     r.valid = false;

//     return r;
//   }

//   // -----------------------------
//   // deviation %
//   // -----------------------------

//   r.deviation_pct =
//     (r.current_rate - baselineRate)
//     / baselineRate
//     * 100.0f;

//   // -----------------------------
//   // estrus decision
//   // -----------------------------

//   r.estrus =
//     (r.deviation_pct >= sysConfig.estrus_threshold_pct);

//   r.valid = true;

//   return r;
// }
