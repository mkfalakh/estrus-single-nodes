#include "storage_stats.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "sd_manager.h"
#include "logger.h"
#include <SD.h>

#define CSV_BUFFER_SIZE 128
#define MAX_PARTITIONS  24

uint32_t csvRowsWritten = 0;

// =====================================================
// PRE-COMPUTED BASELINE (in RAM, per partition)
// =====================================================

struct PrecomputedBaseline {
  float    rate;
  uint32_t samples;
  bool     valid;
};

static PrecomputedBaseline precomputed[MAX_PARTITIONS];

// =====================================================
// PARTITION
// =====================================================

static uint8_t getPartitionFromHour(uint8_t hour) {

  if (sysConfig.partition_hours == 0)
    return 0;

  return (hour / sysConfig.partition_hours);
}

// =====================================================
// TIMESTAMP PARSER
// format:
// 2026-06-10 14:35:00
// =====================================================

static int parseHour(
  const char *timestamp) {

  if (!timestamp)
    return -1;

  if (strlen(timestamp) < 13)
    return -1;

  char hh[3];

  hh[0] = timestamp[11];
  hh[1] = timestamp[12];
  hh[2] = '\0';

  return atoi(hh);
}

// =====================================================
// CSV FIELD
// =====================================================

static bool getField(
  const char *line,
  int fieldIndex,
  char *out,
  size_t outSize) {

  if (!line || !out)
    return false;

  int currentField = 0;

  const char *start = line;
  const char *end = line;

  while (*end) {

    if (*end == ',' || *end == '\n') {

      if (currentField == fieldIndex) {

        size_t len =
          min(
            (size_t)(end - start),
            outSize - 1);

        memcpy(
          out,
          start,
          len);

        out[len] = '\0';

        return true;
      }

      currentField++;

      start = end + 1;
    }

    end++;
  }

  // field terakhir

  if (currentField == fieldIndex) {

    size_t len =
      min(
        strlen(start),
        outSize - 1);

    memcpy(
      out,
      start,
      len);

    out[len] = '\0';

    return true;
  }

  return false;
}

// =====================================================
// FILE NAME
// =====================================================

static String getHistoricalFile(int daysAgo) {

  if (!SYS.rtc_ok) {

    return "";
  }

  DateTime now = getNow();

  uint32_t ts = now.unixtime();

  ts -= (daysAgo * 86400UL);

  DateTime d(ts);

  char path[64];

  snprintf(
    path,
    sizeof(path),
    "/data/%04d-%02d-%02d.csv",
    d.year(),
    d.month(),
    d.day());

  // logToFile(
  //   "📂 Historical file: %s",
  //   path);

  return String(path);
}

// =====================================================
// BASELINE RECOMPUTE
// Scans all historical files in one pass, accumulating
// standing/total counts for every partition at once.
// =====================================================
void triggerBaselineRecompute() {

  for (int p = 0; p < MAX_PARTITIONS; p++) {
    precomputed[p].valid = false;
  }

  if (!SYS.rtc_ok || sysConfig.partition_hours == 0) {
    logToFile("⚠️ Baseline recompute skipped: RTC/partition not ready");
    return;
  }

  uint32_t standingCounts[MAX_PARTITIONS] = {0};
  uint32_t totalCounts[MAX_PARTITIONS]    = {0};

  if (!takeSDMutex("recompute", pdMS_TO_TICKS(2000))) {
    logToFile("⚠️ Baseline recompute: SD mutex timeout");
    return;
  }

  uint16_t yieldCtr = 0;

  for (int d = 1; d <= sysConfig.retention_days; d++) {

    String filename = getHistoricalFile(d);

    // logToFile("📂 Baseline file: %s", filename.c_str());

    if (filename == "") {
      break;
    }

    if (!SD.exists(filename)) {
      // logToFile("⚠️ File not found");
      continue;
    }

    File f = SD.open(filename);

    if (!f) {
      continue;
    }

    // skip header
    f.readStringUntil('\n');

    char line[CSV_BUFFER_SIZE];

    while (f.available()) {

      size_t len =
        f.readBytesUntil('\n', line, sizeof(line) - 1);

      line[len] = '\0';

      if (len < 10)
        continue;

      char timestamp[24];

      if (!getField(line, 2, timestamp, sizeof(timestamp)))
        continue;

      int hour = parseHour(timestamp);

      if (hour < 0)
        continue;

      uint8_t p = getPartitionFromHour(hour);

      if (p >= MAX_PARTITIONS)
        continue;

      char s1buf[4];
      char s2buf[4];

      if (!getField(line, 3, s1buf, sizeof(s1buf)))
        continue;

      if (!getField(line, 4, s2buf, sizeof(s2buf)))
        continue;

      bool isStanding = (atoi(s1buf) && atoi(s2buf));

      totalCounts[p]++;

      if (isStanding)
        standingCounts[p]++;

      // yield every 200 lines so non-SD tasks (LED, buzzer, logger) get
      // CPU time; SD mutex stays held to keep all file handles valid
      if (++yieldCtr >= 200) {
        yieldCtr = 0;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    f.close();
  }

  giveSDMutex();

  // Store results for all partitions
  uint8_t populated = 0;

  for (int p = 0; p < MAX_PARTITIONS; p++) {

    if (totalCounts[p] == 0)
      continue;

    precomputed[p].rate    = (float)standingCounts[p] / (float)totalCounts[p];
    precomputed[p].samples = totalCounts[p];
    precomputed[p].valid   = true;
    populated++;
  }

  logToFile("📊 Baseline recomputed: %u partitions populated", populated);
}

// =====================================================
// GET CACHED BASELINE (RAM read only, zero SD access)
// =====================================================

bool getCachedBaseline(uint8_t partition,
                       float   &baselineRate,
                       uint32_t &baselineSamples) {

  if (partition >= MAX_PARTITIONS)
    return false;

  if (!precomputed[partition].valid)
    return false;

  baselineRate    = precomputed[partition].rate;
  baselineSamples = precomputed[partition].samples;

  return true;
}
