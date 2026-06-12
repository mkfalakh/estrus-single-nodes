#include "system_state.h"
#include "storage_stats.h"

SystemState SYS = {

  .sd_ok = false,
  .rtc_ok = false,
  .sensor_ok = false,
  .sensor_dirty = false,

  .battery_pct = 0,
  .voltage = 0,
  .current = 0,
  .power = 0,

  .sensor1 = false,
  .sensor2 = false,
  .sensor1_dirty = false,
  .sensor2_dirty = false,

  .current_rate = 0,
  .baseline_rate = 0,
  .deviation_pct = 0,
  .estrus = false,
  .partition = 0,
  .baseline_samples = 0,

  .buzzer_active = false,
  .alarm_ack = false,
  .last_alarm_ts = 0
};

static bool lastEstrus = false;

// ========================
// WRITE
// ========================
void sysSetSD(bool ok) {
  SYS.sd_ok = ok;
}
void sysSetRTC(bool ok) {
  SYS.rtc_ok = ok;
}
void sysSetSensor(bool ok) {
  SYS.sensor_ok = ok;
}

void sysSetPower(float pct, float v, float c, float p) {
  SYS.battery_pct = pct;
  SYS.voltage = v;
  SYS.current = c;
  SYS.power = p;
}

void sysSetEstrusResult(
  const EstrusResult &r) {

  SYS.current_rate =
    r.current_rate;

  SYS.baseline_rate =
    r.baseline_rate;

  SYS.deviation_pct =
    r.deviation_pct;

  SYS.partition =
    r.partition;

  SYS.baseline_samples =
    r.baseline_samples;

  SYS.estrus =
    r.estrus;
}

void sysSetSensorState(bool s1, bool s2, bool d1, bool d2) {

  SYS.sensor1 = s1;
  SYS.sensor2 = s2;

  SYS.sensor1_dirty = d1;
  SYS.sensor2_dirty = d2;
}

// ========================
// ALARM CONTROL
// ========================
void sysStartAlarm() {
  SYS.buzzer_active = true;

  SYS.last_alarm_ts = millis();
}

void sysStopAlarm() {
  SYS.buzzer_active = false;
}

void acknowledgeAlarm() {
  SYS.alarm_ack = true;

  sysStopAlarm();
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
bool sysIsError() {
  return (!SYS.sd_ok || !SYS.rtc_ok || !SYS.sensor_ok);
}

bool sysIsLowBattery() {
  return (SYS.battery_pct < 20);  // masih hardcode. bisa dipindah jadi config (sysConfig.low_battery_pct).
}

bool sysIsAlarm() {
  return SYS.buzzer_active;
}

bool sysIsEstrus() {
  return SYS.estrus;
}

void sysSetSensorHealth(bool ok) {

  SYS.sensor_ok = ok;
}

void sysSetSensorDirty(bool dirty) {

  SYS.sensor_dirty = dirty;
}

bool sysIsSensorDirty() {

  return SYS.sensor_dirty;
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
