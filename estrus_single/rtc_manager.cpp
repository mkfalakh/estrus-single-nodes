#include "rtc_manager.h"
#include "system_state.h"
#include "logger.h"

#define TZ_OFFSET 7 * 3600
RTC_DS3231 rtc;

DateTime getNow() {
  // return rtc.now();

  DateTime utc = rtc.now();

  return DateTime(
    utc.unixtime() + TZ_OFFSET);
}

String nowStr() {
  DateTime now = getNow();
  char buf[20];
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
    Serial.println("❌ RTC init gagal!");
    sysSetRTC(false);
    return false;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  Serial.println("✅ RTC OK");
  sysSetRTC(true);
  return true;
}
