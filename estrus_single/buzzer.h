#pragma once
#include <Arduino.h>

enum BuzzerPattern {

  BUZZER_NONE,

  BUZZER_LOW_BATTERY,

  BUZZER_ESTRUS,

  BUZZER_DOUBLE_CLICK,

  BUZZER_LONG_PRESS,

  BUZZER_STOP_CONFIRM,

  BUZZER_BOOT
};

void initBuzzer();
void buzzerOn();
void buzzerOff();
void checkIntervalTrigger();

void buzzerTask(void *pv);

void buzzerPlay(BuzzerPattern pattern);
void buzzerStop();

bool shouldAlarm();
void acknowledgeAlarm();
bool isFaultAlarm();
