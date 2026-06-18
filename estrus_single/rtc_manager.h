#pragma once
#include <RTClib.h>

extern RTC_DS3231 rtc;

bool initRTC();
DateTime getNow();
String todayDateStr(); // "YYYY-MM-DD"
String nowStr(); // "YYYY-MM-DD HH:MM:SS"

void adjustRTC(); // adjust RTC on first boot
void resetRTC(); // reset rtc | DEVELOPMENT ONLY

void saveRTCSyncState(bool synced);
bool loadRTCSyncState();

void saveRTCEpoch(uint32_t epoch);
uint32_t loadRTCEpoch();

bool checkRTCHealth();
