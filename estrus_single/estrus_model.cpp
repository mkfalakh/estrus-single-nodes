#include "estrus_model.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "storage_stats.h"
#include "sd_manager.h"
#include "csv_reverse.h"
#include "logger.h"
#include <SD.h>

// =====================================================
// SLIDING WINDOW CONFIG
// 1.5 h at the minimum interval (10 s) = 540 samples max
// =====================================================

#define DETECT_WINDOW_MAX  540   // max buffer entries (covers 1.5 h at 10 s interval)
#define MAX_PARTITIONS     24

static bool     slidingBuf[DETECT_WINDOW_MAX];   // sensor2_state samples
static bool     dirtyBuf[DETECT_WINDOW_MAX];     // d1||d2 per sample
static uint16_t bufHead  = 0;
static uint16_t bufCount = 0;

static uint8_t  lastDay       = 255;
static uint8_t  lastPartition = 255;

// =====================================================
// HELPERS
// =====================================================

#define DETECT_WINDOW_H  1.5f  // sliding window duration in hours

static uint16_t windowSize()
{
  if (sysConfig.record_interval_sec == 0) return 1;
  uint32_t sz = (uint32_t)(DETECT_WINDOW_H * 3600.0f) / sysConfig.record_interval_sec;
  if (sz < 1)                  sz = 1;
  if (sz > DETECT_WINDOW_MAX)  sz = DETECT_WINDOW_MAX;
  return (uint16_t)sz;
}

static uint8_t getPartitionIndex()
{
  if (sysConfig.partition_hours == 0) return 0;
  return getNow().hour() / sysConfig.partition_hours;
}

// =====================================================
// SLIDING WINDOW UPDATE
// =====================================================

void resetSlidingWindow()
{
  bufHead  = 0;
  bufCount = 0;
}

uint16_t getSlidingWindowCount() { return bufCount; }
uint16_t getSlidingWindowSize()  { return windowSize(); }

// =====================================================
// PREFILL SLIDING WINDOW FROM CSV (called once at boot)
// Reads the last N clean records from today's CSV
// (falls back to yesterday if today has insufficient data)
// so the window is full immediately after reboot.
// Must be called before startTasks() — no mutex needed.
// =====================================================

static int prefillFromFile(const String &path, uint16_t need,
                            char lines[][160])
{
  if (!SD.exists(path)) return 0;
  File f = SD.open(path);
  if (!f) return 0;
  bool hasNext = false;
  int got = readCsvPage(f, lines, 0, need, hasNext);
  f.close();
  return got;
}

static bool parseField(const char *line, int idx, char *out, size_t outSize)
{
  int cur = 0;
  const char *s = line, *e = line;
  while (*e) {
    if (*e == ',' || *e == '\n') {
      if (cur == idx) {
        size_t len = e - s;
        if (len >= outSize) len = outSize - 1;
        memcpy(out, s, len);
        out[len] = '\0';
        return true;
      }
      cur++;
      s = e + 1;
    }
    e++;
  }
  if (cur == idx) {
    size_t len = strlen(s);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, s, len);
    out[len] = '\0';
    return true;
  }
  return false;
}

void prefillSlidingWindow()
{
  if (!SYS.rtc_ok || !SYS.sd_ok) return;

  uint16_t need = windowSize();
  if (need == 0) return;

  // readCsvPage returns newest-first; collect into temp array then inject
  // oldest-first so the ring buffer order matches live ingestion order.
  // Allocate on heap to avoid blowing the setup() stack (~160 * need bytes).
  char (*lines)[160] = (char (*)[160])malloc(need * 160);
  if (!lines) {
    logToFile("⚠️ prefill: malloc failed");
    return;
  }

  DateTime now = getNow();
  char todayPath[32], yestPath[32];
  snprintf(todayPath, sizeof(todayPath), "/data/%04d-%02d-%02d.csv",
           now.year(), now.month(), now.day());
  DateTime yest(now.unixtime() - 86400UL);
  snprintf(yestPath, sizeof(yestPath), "/data/%04d-%02d-%02d.csv",
           yest.year(), yest.month(), yest.day());

  int got = prefillFromFile(todayPath, need, lines);

  // if today not enough, top up from yesterday
  if ((uint16_t)got < need) {
    uint16_t remaining = need - (uint16_t)got;
    char (*extra)[160] = (char (*)[160])malloc(remaining * 160);
    if (extra) {
      int extra_got = prefillFromFile(yestPath, remaining, extra);
      // append yesterday rows after today rows
      for (int i = 0; i < extra_got && got < (int)need; i++) {
        memcpy(lines[got++], extra[i], 160);
      }
      free(extra);
    }
  }

  if (got == 0) {
    free(lines);
    logToFile("⚠️ prefill: no CSV data available");
    return;
  }

  // inject oldest-first (readCsvPage gives newest at index 0, so reverse)
  char s2buf[4], d1buf[4], d2buf[4];
  int injected = 0, skipped = 0;

  for (int i = got - 1; i >= 0; i--) {
    const char *row = lines[i];

    bool d1 = parseField(row, 5, d1buf, sizeof(d1buf)) && atoi(d1buf);
    bool d2 = parseField(row, 6, d2buf, sizeof(d2buf)) && atoi(d2buf);

    if (d1 || d2) { skipped++; continue; }

    bool s2 = parseField(row, 4, s2buf, sizeof(s2buf)) && atoi(s2buf);
    updateSensor2(s2, false, false);
    injected++;
  }

  free(lines);

  logToFile("📂 prefill: injected=%d skipped_dirty=%d window=%d/%d",
            injected, skipped, bufCount, need);
}

void updateSensor2(bool s2, bool d1, bool d2)
{
  slidingBuf[bufHead] = s2;
  dirtyBuf[bufHead]   = (d1 || d2);
  bufHead = (bufHead + 1) % DETECT_WINDOW_MAX;
  if (bufCount < DETECT_WINDOW_MAX) bufCount++;
}

// =====================================================
// TIME TRANSITIONS (day rollover only)
// =====================================================

void checkTimeTransitions()
{
  if (!SYS.rtc_ok) return;

  DateTime now = getNow();
  uint8_t  newDay       = now.day();
  uint8_t  newPartition = getPartitionIndex();

  if (lastDay == 255) {
    lastDay       = newDay;
    lastPartition = newPartition;
    return;
  }

  if (newDay != lastDay) {
    triggerBaselineRecompute();
    resetAlarmAcknowledgement();
    logToFile("📅 New day → baseline recomputed");
  }

  lastDay       = newDay;
  lastPartition = newPartition;
}

// =====================================================
// CALENDAR GATE — parse injection_date, return cycle day
// Returns 0 if injection_date not set or RTC unavailable.
// =====================================================

static int cycleDay()
{
  if (strlen(sysConfig.injection_date) != 10) return 0;
  if (!SYS.rtc_ok) return 0;

  char buf[5];
  strncpy(buf, sysConfig.injection_date,     4); buf[4] = '\0'; int sy = atoi(buf);
  strncpy(buf, sysConfig.injection_date + 5, 2); buf[2] = '\0'; int sm = atoi(buf);
  strncpy(buf, sysConfig.injection_date + 8, 2); buf[2] = '\0'; int sd = atoi(buf);

  DateTime inj(sy, sm, sd, 0, 0, 0);
  int32_t diffSec = (int32_t)(getNow().unixtime() - inj.unixtime());
  if (diffSec < 0) return 0;
  return (diffSec / 86400) + 1;
}

// =====================================================
// MAIN EVALUATION
// =====================================================

EstrusResult evaluateEstrus()
{
  EstrusResult r = {};

  uint16_t wSize = windowSize();

  if (bufCount < wSize) return r;  // window not yet full

  // --------------------------------------------------
  // 1. Scan sliding window: on_frac, rises, dirty flag
  // --------------------------------------------------
  uint16_t onCount  = 0;
  uint16_t rises    = 0;
  bool     winDirty = false;
  bool     prev     = false;

  for (uint16_t i = 0; i < wSize; i++) {
    uint16_t idx = (uint16_t)(bufHead + DETECT_WINDOW_MAX - wSize + i) % DETECT_WINDOW_MAX;
    bool val = slidingBuf[idx];
    if (val)          onCount++;
    if (val && !prev) rises++;
    if (dirtyBuf[idx]) winDirty = true;
    prev = val;
  }

  float duration_h  = (float)wSize * sysConfig.record_interval_sec / 3600.0f;
  float on_frac     = (float)onCount / (float)wSize;
  float rises_per_h = (duration_h > 0.0f) ? ((float)rises / duration_h) : 0.0f;

  r.current_rate = on_frac * 100.0f;

  // --------------------------------------------------
  // 2. Quality gate
  // --------------------------------------------------
  if (winDirty) {
    r.untrusted = true;
    return r;
  }

  // --------------------------------------------------
  // 3. Baseline (median + MAD)
  // --------------------------------------------------
  uint8_t  p = getPartitionIndex();
  float    medianRate, madRate;
  uint16_t nWindows;

  if (!getCachedBaseline(p, medianRate, madRate, nWindows)) return r;

  r.baseline_rate    = medianRate * 100.0f;
  r.baseline_windows = nWindows;

  if (nWindows < sysConfig.min_baseline_windows) {
    r.valid = false;
    return r;
  }

  // --------------------------------------------------
  // 4. Robust z-score
  // --------------------------------------------------
  r.z_score = (on_frac - medianRate) / madRate;

  float z_threshold = sysConfig.estrus_threshold_pct / 100.0f * 4.0f;
  if (z_threshold < 0.1f) z_threshold = 0.1f;

  r.deviation_pct = (r.z_score / z_threshold) * 100.0f;

  // --------------------------------------------------
  // 5. Calendar gate (days 20-21; bypass if no injection date)
  // --------------------------------------------------
  int cd         = cycleDay();
  bool calendarOk = (cd == 0) || (cd >= 20 && cd <= 21);

  r.estrus = (r.z_score >= z_threshold) && calendarOk;
  r.valid  = true;

  (void)rises_per_h;  // available for future use / debug logging

  return r;
}
