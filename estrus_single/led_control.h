#pragma once
#include <Arduino.h>

enum LedPattern {

  LED_NONE,

  LED_IDLE,

  LED_ESTRUS,

  LED_FAULT,

  LED_SENSOR_DIRTY,

  LED_NO_ACTIVITY,

  LED_LOW_BATTERY,

  LED_RESTART,

  LED_FACTORY_RESET
};

void initLED();

void ledTask(void *pv);
void setLED(uint8_t r, uint8_t g, uint8_t b);

extern volatile LedPattern ledPattern;
