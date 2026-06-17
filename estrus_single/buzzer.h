#pragma once
#include <Arduino.h>

enum BuzzerPattern {

  BUZZER_NONE,

  BUZZER_LOW_BATTERY,

  BUZZER_ESTRUS,

  BUZZER_DOUBLE_CLICK,

  BUZZER_STOP_CONFIRM
};

void initBuzzer();
void checkIntervalTrigger();

void buzzerTask(void *pv);

void buzzerPlay(BuzzerPattern pattern);
void buzzerStop();

bool shouldAlarm();
void acknowledgeAlarm();
bool isFaultAlarm();
