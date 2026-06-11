#include "storage_stats.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include <SD.h>

#define CSV_BUFFER_SIZE 128

struct BaselineCache {

  uint8_t partition;
  uint8_t day;
  float baseline;
  uint32_t samples;
  bool valid;
};

static BaselineCache baselineCache = {

  255,
  255,
  0,
  0,
  false
};

uint32_t csvRowsWritten = 0;

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
    "/data/%s-%04d-%02d-%02d.csv",
    sysConfig.node_id,
    d.year(),
    d.month(),
    d.day());

  return String(path);
}

// =====================================================
// BASELINE
// =====================================================

bool loadPartitionBaseline(uint8_t partition,
                           float &baselineRate,
                           uint32_t &baselineSamples) {

  baselineRate = 0.0f;
  baselineSamples = 0;
  uint32_t standingCount = 0;
  uint32_t totalCount = 0;

  DateTime now = getNow();
  uint8_t today = now.day();

  if (baselineCache.valid && baselineCache.partition == partition &&

      baselineCache.day == today) {
    baselineRate =
      baselineCache.baseline;

    baselineSamples =
      baselineCache.samples;

    baselineCache.partition =
      partition;

    baselineCache.day =
      today;

    baselineCache.baseline =
      baselineRate;

    baselineCache.samples =
      baselineSamples;

    baselineCache.valid =
      true;

    return true;
  }

  // ==========================================
  // LOOP HISTORICAL FILES
  // ==========================================

  for (
    int d = 1;
    d <= sysConfig.retention_days;
    d++) {

    String filename = getHistoricalFile(d);

    if (filename == "")
      break;

    if (!SD.exists(filename))
      continue;

    File f =
      SD.open(filename);

    if (!f)
      continue;

    // skip header

    f.readStringUntil('\n');

    char line[CSV_BUFFER_SIZE];

    while (f.available()) {

      size_t len =
        f.readBytesUntil(
          '\n',
          line,
          sizeof(line) - 1);

      line[len] = '\0';

      if (len < 10)
        continue;

      char timestamp[24];

      if (!getField(
            line,
            2,
            timestamp,
            sizeof(timestamp)))
        continue;

      int hour =
        parseHour(
          timestamp);

      if (hour < 0)
        continue;

      if (
        getPartitionFromHour(hour)
        != partition)
        continue;

      char s1buf[4];
      char s2buf[4];

      if (!getField(
            line,
            3,
            s1buf,
            sizeof(s1buf)))
        continue;

      if (!getField(
            line,
            4,
            s2buf,
            sizeof(s2buf)))
        continue;

      bool sensor1 =
        atoi(s1buf);

      bool sensor2 =
        atoi(s2buf);

      bool standing =
        (sensor1 && sensor2);

      totalCount++;

      if (standing)
        standingCount++;
    }

    f.close();
  }

  // ==========================================
  // RESULT
  // ==========================================

  if (totalCount == 0)
    return false;

  baselineRate = (float)standingCount / (float)totalCount;

  baselineSamples = totalCount;

  return true;
}

// INVALID BASELINE CACHE
void invalidateBaselineCache() {

  baselineCache.valid = false;
}
