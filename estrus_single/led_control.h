#pragma once
#include <Arduino.h>

enum LedState {
  LED_NORMAL,
  LED_LOW_BAT,
  LED_ALARM,
  LED_ERROR
};

void initLED();

void ledTask(void *pv);
void setLED(uint8_t r, uint8_t g, uint8_t b);
