#include "storage_stats.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "sd_manager.h"
#include "logger.h"
#include <SD.h>

#define CSV_BUFFER_SIZE  128
#define MAX_PARTITIONS   24
#define MAX_RETENTION    14   // max retention_days

uint32_t csvRowsWritten = 0;

// =====================================================
// PRE-COMPUTED BASELINE (in RAM, per partition)
// median+MAD of sensor2_state on_frac across healthy windows
// =====================================================

struct PrecomputedBaseline {
  float    median_on_frac;
  float    mad_on_frac;    // normalized MAD (× 1.4826)
  uint16_t n_windows;
  bool     valid;
};

static PrecomputedBaseline precomputed[MAX_PARTITIONS];

// =====================================================
// CSV FIELD PARSER
// =====================================================

static bool getField(
  const char *line,
  int         fieldIndex,
  char       *out,
  size_t      outSize)
{
  if (!line || !out) return false;

  int currentField = 0;
  const char *start = line;
  const char *end   = line;

  while (*end) {
    if (*end == ',' || *end == '\n') {
      if (currentField == fieldIndex) {
        size_t len = min((size_t)(end - start), outSize - 1);
        memcpy(out, start, len);
        out[len] = '\0';
        return true;
      }
      currentField++;
      start = end + 1;
    }
    end++;
  }

  if (currentField == fieldIndex) {
    size_t len = min(strlen(start), outSize - 1);
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
  }

  return false;
}

// =====================================================
// TIMESTAMP → HOUR
// format: YYYY-MM-DD HH:MM:SS
// =====================================================

static int parseHour(const char *timestamp)
{
  if (!timestamp || strlen(timestamp) < 13) return -1;
  char hh[3] = { timestamp[11], timestamp[12], '\0' };
  return atoi(hh);
}

// =====================================================
// HISTORICAL FILE PATH
// =====================================================

static String getHistoricalFile(int daysAgo)
{
  if (!SYS.rtc_ok) return "";

  DateTime now = getNow();
  uint32_t ts  = now.unixtime() - (daysAgo * 86400UL);
  DateTime d(ts);

  char path[32];
  snprintf(path, sizeof(path), "/data/%04d-%02d-%02d.csv",
           d.year(), d.month(), d.day());
  return String(path);
}

// =====================================================
// SORT (insertion sort — small arrays only)
// =====================================================

static void sortFloats(float *arr, uint8_t n)
{
  for (uint8_t i = 1; i < n; i++) {
    float key = arr[i];
    int8_t j  = i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

static float medianOf(float *sorted, uint8_t n)
{
  if (n == 0) return 0.0f;
  if (n & 1)  return sorted[n / 2];
  return (sorted[n / 2 - 1] + sorted[n / 2]) * 0.5f;
}

// =====================================================
// BASELINE RECOMPUTE
// One pass over retention window: per partition, collect
// per-window sensor2 on_frac (skip dirty windows).
// Then sort and compute median+MAD per partition.
// =====================================================

void triggerBaselineRecompute()
{
  for (int p = 0; p < MAX_PARTITIONS; p++) precomputed[p].valid = false;

  if (!SYS.rtc_ok || sysConfig.partition_hours == 0) {
    logToFile("⚠️ Baseline recompute skipped: RTC/partition not ready");
    return;
  }

  // per-partition per-day accumulators (reset per file)
  uint32_t onCount[MAX_PARTITIONS];
  uint32_t totalCount[MAX_PARTITIONS];
  bool     hasDirty[MAX_PARTITIONS];

  // collected on_frac values across all files (≤ MAX_RETENTION per partition)
  float    values[MAX_PARTITIONS][MAX_RETENTION];
  uint8_t  valueCnt[MAX_PARTITIONS];
  memset(valueCnt, 0, sizeof(valueCnt));

  if (!takeSDMutex("recompute", pdMS_TO_TICKS(2000))) {
    logToFile("⚠️ Baseline recompute: SD mutex timeout");
    return;
  }

  uint16_t yieldCtr = 0;

  for (int d = 1; d <= sysConfig.retention_days; d++) {
    String filename = getHistoricalFile(d);
    if (filename == "" || !SD.exists(filename)) continue;

    File f = SD.open(filename);
    if (!f) continue;

    f.readStringUntil('\n');  // skip header

    memset(onCount,   0, sizeof(onCount));
    memset(totalCount, 0, sizeof(totalCount));
    memset(hasDirty,  0, sizeof(hasDirty));

    char line[CSV_BUFFER_SIZE];

    while (f.available()) {
      size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
      line[len] = '\0';
      if (len < 10) continue;

      char tsBuf[24];
      if (!getField(line, 2, tsBuf, sizeof(tsBuf))) continue;
      int hour = parseHour(tsBuf);
      if (hour < 0) continue;

      uint8_t p = hour / sysConfig.partition_hours;
      if (p >= MAX_PARTITIONS) continue;

      // sensor2_state = field 4
      char s2buf[4];
      if (!getField(line, 4, s2buf, sizeof(s2buf))) continue;

      // dirty flags = fields 5 and 6
      char d1buf[4], d2buf[4];
      bool d1 = getField(line, 5, d1buf, sizeof(d1buf)) && atoi(d1buf);
      bool d2 = getField(line, 6, d2buf, sizeof(d2buf)) && atoi(d2buf);

      if (d1 || d2) hasDirty[p] = true;

      totalCount[p]++;
      if (atoi(s2buf)) onCount[p]++;

      if (++yieldCtr >= 200) {
        yieldCtr = 0;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    f.close();

    // commit trusted windows for this day
    for (int p = 0; p < MAX_PARTITIONS; p++) {
      if (totalCount[p] == 0 || hasDirty[p]) continue;
      if (valueCnt[p] >= MAX_RETENTION) continue;
      values[p][valueCnt[p]++] = (float)onCount[p] / (float)totalCount[p];
    }
  }

  giveSDMutex();

  // compute median + MAD for each partition
  uint8_t populated = 0;

  for (int p = 0; p < MAX_PARTITIONS; p++) {
    uint8_t n = valueCnt[p];
    if (n == 0) continue;

    sortFloats(values[p], n);
    float med = medianOf(values[p], n);

    // compute |val - median|
    float absDevs[MAX_RETENTION];
    for (uint8_t i = 0; i < n; i++) absDevs[i] = fabsf(values[p][i] - med);
    sortFloats(absDevs, n);
    float rawMad = medianOf(absDevs, n);
    float normMad = rawMad * 1.4826f;
    if (normMad < 1e-4f) normMad = 1e-4f;  // floor to avoid division by zero

    precomputed[p].median_on_frac = med;
    precomputed[p].mad_on_frac    = normMad;
    precomputed[p].n_windows      = n;
    precomputed[p].valid          = true;
    populated++;
  }

  logToFile("📊 Baseline recomputed: %u partitions populated", populated);
}

// =====================================================
// GET CACHED BASELINE (RAM only, zero SD access)
// =====================================================

bool getCachedBaseline(uint8_t   partition,
                       float    &medianRate,
                       float    &madRate,
                       uint16_t &nWindows)
{
  if (partition >= MAX_PARTITIONS) return false;
  if (!precomputed[partition].valid)  return false;

  medianRate = precomputed[partition].median_on_frac;
  madRate    = precomputed[partition].mad_on_frac;
  nWindows   = precomputed[partition].n_windows;
  return true;
}
