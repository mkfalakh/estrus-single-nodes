#pragma once
#include <Arduino.h>

void checkFreeSD();

void initLogger();
void logToFile(const char *fmt, ...);
void logToFile(String msg);
void loggerTask(void *pv);

void setSDReadyForLog(bool ready);
bool initSDCard();

extern SemaphoreHandle_t sdMutex;
extern QueueHandle_t logQueue;
extern bool sdReadyForLog;