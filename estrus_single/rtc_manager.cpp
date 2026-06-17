#include "rtc_manager.h"
#include "system_state.h"
#include "logger.h"
#include "config_runtime.h"
#include <Preferences.h>

#define RTC_SYNC_KEY "rtc_sync"

RTC_DS3231 rtc;

// simpan status sinkron rtc ke nvs
void saveRTCSyncState(bool synced) {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    return;
  }

  prefs.putBool(RTC_SYNC_KEY, synced);

  prefs.end();
}

// load status sinkron rtc
bool loadRTCSyncState() {

  Preferences prefs;

  if (!prefs.begin("sapi", true)) {
    return false;
  }

  bool synced =
    prefs.getBool(RTC_SYNC_KEY, false);

  prefs.end();

  return synced;
}


DateTime getNow() {

  return rtc.now();
}

String nowStr() {
  DateTime now = getNow();
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  return String(buf);
}

String todayDateStr() {
  DateTime now = getNow();
  char buf[11];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           now.year(), now.month(), now.day());
  return String(buf);
}


bool initRTC() {

  if (!rtc.begin()) {

    logToFile("❌ RTC init gagal");

    sysSetRTC(false);

    return false;
  }

  SYS.rtc_ever_synced =
    loadRTCSyncState();

  if (rtc.lostPower()) {

    logToFile(
      "⚠ RTC lost power");

    SYS.rtc_ever_synced = false;

    saveRTCSyncState(false);

    sysSetRTC(false);

    return false;
  }

  sysSetRTC(true);

  Serial.println(
    "✅ RTC OK");

  return true;
}


// adjust rtc time manually | run 1x in Setup()
void adjustRTC() {

  DateTime now = rtc.now();

  // set your local time here | year, month, day, hour, minute, second
  rtc.adjust(
    DateTime(2026, 6, 14, 10, 0, 0));

  Serial.printf(
    "RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
    now.year(),
    now.month(),
    now.day(),
    now.hour(),
    now.minute(),
    now.second());

  Serial.printf(
    "Lost Power: %d\n",
    rtc.lostPower());
}


// reset rtc time to 2000-01-01 01:01:01 | DEVELOPMENT ONLY
void resetRTC() {

  DateTime now = rtc.now();

  // set your local time here | year, month, day, hour, minute, second
  rtc.adjust(
    DateTime(2000, 1, 1, 1, 1, 1));

  Serial.printf(
    "RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
    now.year(),
    now.month(),
    now.day(),
    now.hour(),
    now.minute(),
    now.second());

  Serial.printf(
    "Lost Power: %d\n",
    rtc.lostPower());
}
