#include "system_state.h"
#include "storage_stats.h"
#include "logger.h"

SystemState SYS = {

  .sd_ok = false,
  .rtc_ok = false,
  .ina_ok = false,

  .battery_pct = 0,
  .battery_voltage = 0,
  .bus_voltage = 0,
  .current = 0,
  .power = 0,

  .sensor1 = false,
  .sensor2 = false,
  .sensor1_dirty = false,
  .sensor2_dirty = false,
  .sensor1_no_activity = false,
  .sensor2_no_activity = false,

  .current_rate     = 0,
  .baseline_rate    = 0,
  .deviation_pct    = 0,
  .estrus           = false,
  .partition        = 0,
  .baseline_windows = 0,

  .alarm_active = false,
  .fault_alarm_muted = false,
  .alarm_ack = false,
  .last_alarm_ts = 0,

  .rtc_ever_synced = false,
  .rtc_drift_seconds = 0,
  .last_synced_epoch = 0,
  .last_sync_millis = 0
};

static bool lastEstrus = false;

volatile unsigned long sdRecoveredAt = 0;

// ========================
// WRITE SYSTEM
// ========================
void sysSetSD(bool ok) {
  SYS.sd_ok = ok;
}
void sysSetRTC(bool ok) {
  SYS.rtc_ok = ok;
}
void sysSetINA(bool ok) {
  SYS.ina_ok = ok;
}

void sysSetSensor1Dirty(bool d1) {
  SYS.sensor1_dirty = d1;
}

void sysSetSensor2Dirty(bool d2) {
  SYS.sensor2_dirty = d2;
}

void sysSetEstrusResult(const EstrusResult &r) {
  SYS.current_rate     = r.current_rate;
  SYS.baseline_rate    = r.baseline_rate;
  SYS.deviation_pct    = r.deviation_pct;
  SYS.baseline_windows = r.baseline_windows;
  SYS.estrus           = r.estrus;
}

void sysSetSensorState(bool s1, bool s2, bool d1, bool d2) {

  SYS.sensor1 = s1;
  SYS.sensor2 = s2;

  SYS.sensor1_dirty = d1;
  SYS.sensor2_dirty = d2;
}

// BATTERY
void sysSetPower(float pct, float v, float c, float p) {
  SYS.battery_pct = pct;
  SYS.battery_voltage = v;
  SYS.current = c;
  SYS.power = p;
}

bool sysIsLowBattery() {
  return (SYS.battery_pct <= 20);  // masih hardcode. bisa dipindah jadi config (sysConfig.low_battery_pct).
}

// ========================
// ALARM CONTROL
// ========================
void sysSetAlarm(bool value) {

  SYS.alarm_active = value;
}

void sysStartAlarm() {

  SYS.alarm_active = true;

  logToFile(
    "🔊 Alarm Berbunyi");

  SYS.last_alarm_ts = millis();
}

void sysStopAlarm() {

  SYS.alarm_active = false;

  logToFile(
    "🔕 Alarm stopped");
}

void resetAlarmAcknowledgement() {
  SYS.alarm_ack = false;
}

bool isAlarmAcknowledged() {
  return SYS.alarm_ack;
}

// ========================
// READ (HELPER)
// ========================
// HEALTH
bool sysIsSystemFault() {
  return (!SYS.sd_ok || !SYS.rtc_ok || !SYS.ina_ok);
}

bool sysIsAlarm() {
  return SYS.alarm_active;
}

bool sysIsEstrus() {
  return SYS.estrus;
}

bool sysIsSensor1Dirty() {
  return SYS.sensor1_dirty;
}

bool sysIsSensor2Dirty() {
  return SYS.sensor2_dirty;
}

bool sysIsSensor1NoActivity() {
  return SYS.sensor1_no_activity;
}

bool sysIsSensor2NoActivity() {
  return SYS.sensor2_no_activity;
}

// MODEL
float sysGetDeviationPct() {
  return SYS.deviation_pct;
}

float sysGetCurrentRate() {
  return SYS.current_rate;
}

float sysGetBaselineRate() {
  return SYS.baseline_rate;
}

// ========================
// SYS WIFI
// ========================
static unsigned long wifiWakeTs = 0;

void sysTriggerWifiWake() {
  wifiWakeTs = millis();
}

bool sysWifiWakeActive() {
  return (millis() - wifiWakeTs < 2000);
}
