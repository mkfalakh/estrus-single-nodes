#include "rtc_manager.h"

RTC_DS3231 rtc;

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("❌ RTC tidak ditemukan");
    while (1);
  }
}

DateTime getNow() {
  return rtc.now();
}