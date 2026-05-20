#pragma once
#include <Arduino.h>

// ========================
// ENUM STATUS
// ========================
enum SystemHealth {
  SYS_OK,
  SYS_WARN,
  SYS_ERROR
};

enum AnimalState {
  ANIMAL_NORMAL,
  ANIMAL_ESTRUS
};

// ========================
// STRUCT STATE
// ========================
typedef struct {

  // --- health ---
  bool sd_ok;
  bool rtc_ok;
  bool sensor_ok;

  // --- power ---
  float battery_pct;
  float voltage;
  float current;
  float power;

  // --- activity ---
  int a1;
  int a2;
  int total;

  // --- model ---
  float score;
  bool estrus;

  // --- control ---
  bool buzzer_active;
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
void sysSetActivity(int a1, int a2);
void sysSetModel(float score, bool estrus);

void sysTriggerAlarm();
void sysStopAlarm();

// ========================
// API (READ)
// ========================
bool sysIsError();
bool sysIsLowBattery();
bool sysIsAlarm();
bool sysIsEstrus();

// ========================
// WIFI AP
// ========================
extern bool wifiEnabled;
extern unsigned long lastClientTime;
void sysTriggerWifiWake();
bool sysWifiWakeActive();
