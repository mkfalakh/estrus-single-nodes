#include "system_state.h"

SystemState SYS = {
  false, false, false,  // sd, rtc, sensor | default: false,false,false
  0, 0, 0, 0,           // battery, voltage, current, power | default: 0,0,0,0
  0, 0, 0,              // a1, a2, total | default: 0,0,0
  0, false,             // score, estrus | default: 0,false
  false, 0              // buzzer/alarm, last_alarm_ts | default: false,0
};

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

void sysSetActivity(int a1, int a2) {
  SYS.a1 = a1;
  SYS.a2 = a2;
  SYS.total = a1 + a2;
}

void sysSetModel(float score, bool estrus) {
  SYS.score = score;
  SYS.estrus = estrus;

  // jika estrus bunyikan alarm
  // if (estrus) {
  //   sysTriggerAlarm();
  // }
}

// ========================
// ALARM CONTROL
// ========================
void sysTriggerAlarm() {
  SYS.buzzer_active = true;
  SYS.last_alarm_ts = millis();
}

void sysStopAlarm() {
  SYS.buzzer_active = false;
}

// ========================
// READ
// ========================
bool sysIsError() {
  return (!SYS.sd_ok || !SYS.rtc_ok || !SYS.sensor_ok);
}

bool sysIsLowBattery() {
  return (SYS.battery_pct < 20);
}

bool sysIsAlarm() {
  return SYS.buzzer_active;
}

bool sysIsEstrus() {
  return SYS.estrus;
}

// ========================
// WIFI AP
// ========================
bool wifiEnabled = true;
unsigned long lastClientTime = 0;
static unsigned long wifiWakeTs = 0;

void sysTriggerWifiWake() {

  wifiWakeTs = millis();
}

bool sysWifiWakeActive() {

  return (
    millis() - wifiWakeTs < 2000);
}

