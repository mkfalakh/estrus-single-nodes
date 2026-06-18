#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t sdMutex;
extern volatile const char* sdMutexOwner;

bool takeSDMutex(const char* owner, TickType_t timeout = pdMS_TO_TICKS(500));
void giveSDMutex();

void createSDMutex();
bool initSDCard();
void checkFreeSD();

bool remountSDCard();
