#pragma once
#include <Arduino.h>

void initBuzzerAndBtn();
void checkIntervalTrigger();

void buttonTask(void *pv);
void buzzerTask(void *pv);

bool buttonPressed();