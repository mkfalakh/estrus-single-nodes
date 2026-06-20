#include "estrus_model.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "storage_stats.h"
#include "logger.h"

// =====================================================
// SLIDING WINDOW CONFIG
// 1.5 h at the minimum interval (10 s) = 540 samples max
// =====================================================

#define DETECT_WINDOW_MAX  540   // max buffer entries (covers 1.5 h at 10 s interval)
#define DETECT_WINDOW_H    1.5f  // sliding window duration in hours
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
