#pragma once
#include <RTClib.h>

extern RTC_DS3231 rtc;

bool initRTC();
DateTime getNow();
String todayDateStr(); // "YYYY-MM-DD"
String nowStr(); // "YYYY-MM-DD HH:MM:SS"

uint8_t getPartitionIndex();
