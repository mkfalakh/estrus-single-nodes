#pragma once
#include <Arduino.h>
#include "estrus_model.h"

// ========================
// ENUM STATUS
// ========================
enum SystemHealth {
  SYS_OK,
  SYS_WARN,
  SYS_ERROR
};

// ========================
// STRUCT STATE
// ========================
typedef struct {

  // --- health ---
  bool sd_ok;
  bool rtc_ok;
  bool sensor_ok;
  bool sensor_dirty;

  // --- power ---
  float battery_pct;
  float voltage;
  float current;
  float power;

  // --- realtime sensor ---
  bool sensor1;
  bool sensor2;
  bool sensor1_dirty;
  bool sensor2_dirty;

  // --- model estrus ---
  float current_rate;
  float baseline_rate;
  float deviation_pct;
  bool estrus;
  uint8_t partition;
  uint32_t baseline_samples;

  // --- control ---
  bool buzzer_active;
  bool alarm_ack;
  unsigned long last_alarm_ts;

} SystemState;

// ========================
// GLOBAL INSTANCE
// ========================
extern SystemState SYS;

// ========================
// API (WRITE)
// ========================
void sysSetSD(bool ok);
void sysSetRTC(bool ok);
void sysSetSensor(bool ok);
void sysSetPower(float pct, float v, float c, float p);

void sysSetEstrusResult(const EstrusResult &r);
void sysSetSensorState(bool s1, bool s2, bool d1, bool d2);

void sysStartAlarm();
void sysStopAlarm();

// ========================
// API (HELPER)
// ========================
bool sysIsError();
bool sysIsLowBattery();
bool sysIsAlarm();
bool sysIsEstrus();

// SENSOR
void sysSetSensorHealth(bool ok);
void sysSetSensorDirty(bool dirty);
bool sysIsSensorDirty();

// MODEL
float sysGetDeviationPct();
float sysGetCurrentRate();
float sysGetBaselineRate();

// ALARM
void acknowledgeAlarm();
void resetAlarmAcknowledgement();
bool isAlarmAcknowledged();

// ========================
// SYS WIFI
// ========================
void sysTriggerWifiWake();
bool sysWifiWakeActive();
