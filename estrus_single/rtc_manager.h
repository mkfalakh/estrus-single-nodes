#pragma once
#include <RTClib.h>

extern RTC_DS3231 rtc;

void initRTC();
DateTime getNow();