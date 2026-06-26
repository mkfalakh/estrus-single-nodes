#include <stdint.h>
#pragma once
#include <Arduino.h>

#define NODE_ID_MAX 16
#define ANIMAL_ID_MAX 16

typedef struct {
  // ===== DEVICE =====
  // All char arrays are grouped first so each starts at a 4-byte-aligned
  // offset; the ROM strlen/memcpy uses L32I word loads and crashes with
  // LoadStoreAlignment if the source pointer is not 4-byte aligned.
  char node_id[NODE_ID_MAX];   // offset 0  (aligned)
  char animal_id[ANIMAL_ID_MAX]; // offset 16 (aligned)
  char ap_password[32];          // offset 32 (aligned), ends at 64

  // Hormone injection date (YYYY-MM-DD). Leave empty if not set.
  // /api/node/estrus reports cycle_day and is_estrus_window (days 20-21).
  char injection_date[12];       // offset 64 (aligned), ends at 76

  bool prox_active_low;  // LOW / HIGH
  bool alarm_enabled;
  uint8_t led_brightness;
  uint8_t no_activity_timeout_hours;

  // ===== BATTERY ALERT =====
  float current_threshold;
  float power_threshold;

  // ===== MODEL PARAMETER =====
  uint16_t record_interval_sec;
  uint8_t retention_days;
  uint8_t partition_hours;
  float estrus_threshold_pct;
  bool stop_after_alarm;

  uint8_t  min_baseline_windows;   // healthy windows required before z-score is valid (2-48)
  uint16_t dirty_timeout_min;      // minutes sensor must be stuck before marked untrusted (10-480)

} SystemConfig;

extern SystemConfig sysConfig;

extern bool pendingRestart;
extern unsigned long restartAt;

bool setNodeId(const String &id);
extern bool isValidNodeId(const String &id);

bool setAnimalId(const String &id);
extern bool isValidAnimalId(const String &id);

void loadConfig();
void saveConfig();
void resetConfig();

void saveEnergyStats();
void loadEnergyStats();
