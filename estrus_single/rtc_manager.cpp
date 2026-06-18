#include "rtc_manager.h"
#include "system_state.h"
#include "logger.h"
#include "config_runtime.h"
#include <Preferences.h>

RTC_DS3231 rtc;

// simpan epoch rtc ke nvs
void saveRTCEpoch(uint32_t epoch) {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    return;
  }

  prefs.putULong("rtc_epoch", epoch);

  prefs.end();
}

// load epoch RTC yang tersimpan
uint32_t loadRTCEpoch() {

  Preferences prefs;

  if (!prefs.begin("sapi", true)) {
    return 0;
  }

  uint32_t epoch = prefs.getULong("rtc_epoch", 0);

  prefs.end();

  return epoch;
}

// simpan status sinkron rtc ke nvs
void saveRTCSyncState(bool synced) {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    return;
  }

  prefs.putBool("rtc_sync", synced);

  prefs.end();
}

// load status sinkron rtc
bool loadRTCSyncState() {

  Preferences prefs;

  if (!prefs.begin("sapi", true)) {
    return false;
  }

  bool synced = prefs.getBool("rtc_sync", false);

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
  char buf[16];
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

  SYS.rtc_ever_synced = loadRTCSyncState();
  // SYS.last_synced_epoch = loadRTCEpoch();

  logToFile(
    "RTC sync state=%d",

    SYS.rtc_ever_synced);

  if (rtc.lostPower()) {

    DateTime rtcNow = rtc.now();

    logToFile(
      "⚠️ RTC battery lost power");

    logToFile(
      "⚠️ RTC current time: %04d-%02d-%02d %02d:%02d:%02d",

      rtcNow.year(),
      rtcNow.month(),
      rtcNow.day(),

      rtcNow.hour(),
      rtcNow.minute(),
      rtcNow.second());

    logToFile(
      "⚠️ RTC need to sync!");

    sysSetRTC(false);

    return false;
  }

  sysSetRTC(true);

  Serial.println(
    "✅ RTC OK");

  return true;
}


// check RTC health
bool checkRTCHealth() {

  Wire.beginTransmission(0x68);

  if (Wire.endTransmission() != 0) {

    logToFile("⚠️ RTC Wire error!");

    sysSetRTC(false);

    return false;
  }

  DateTime now = rtc.now();

  if (now.year() < 2026 || now.year() > 2100) {

    logToFile("⚠️ RTC year invalid! RTC need to sync!");

    sysSetRTC(false);

    return false;
  }

  sysSetRTC(true);

  return true;
}


// adjust rtc time manually | run 1x in Setup()
void adjustRTC() {

  DateTime now = rtc.now();

  // set your local time here | year, month, day, hour, minute, second
  rtc.adjust(DateTime(2026, 6, 14, 10, 0, 0));

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
