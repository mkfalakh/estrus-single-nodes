#pragma once
#include <Arduino.h>

void initLogger();
void logToFile(const char *fmt, ...);
void logToFile(String msg);
void loggerTask(void *pv);
void setSDReadyForLog(bool ready);

extern QueueHandle_t logQueue;
extern bool sdReadyForLog;
