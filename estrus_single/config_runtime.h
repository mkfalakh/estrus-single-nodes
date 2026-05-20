#pragma once
#include <Arduino.h>

#define NODE_ID_MAX 16

typedef struct {
  // ===== DEVICE =====
  char node_id[NODE_ID_MAX];
  bool prox_active_low;  // LOW / HIGH
  int interval_hours;
  bool buzzer_enabled;

  // ===== BATTERY ALERT =====
  float current_threshold;
  float power_threshold;

  // ===== MODEL PARAMETER =====
  float score_threshold;  // 🔥 utama
  float ratio_trigger;    // R > ?
  int persist_required;   // berapa kali berturut-turut
  float ema_alpha;        // adaptasi baseline

  // ===== SENSOR =====
  int activity_min;   // minimal activity valid
  float balance_min;  // a1 vs a2 ratio

} SystemConfig;

extern SystemConfig sysConfig;

extern bool pendingRestart;
extern unsigned long restartAt;

bool setNodeId(const String &id);
extern bool isValidNodeId(const String &id);
// String getNodeId();

void loadConfig();
void saveConfig();
void resetConfig();
