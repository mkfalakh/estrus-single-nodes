#pragma once
#include <Arduino.h>

// TASK HANDLES
extern TaskHandle_t sensorTaskHandle;
extern TaskHandle_t csvTaskHandle;
extern TaskHandle_t loggerTaskHandle;
extern TaskHandle_t ledTaskHandle;
extern TaskHandle_t buzzerTaskHandle;
extern TaskHandle_t buttonTaskHandle;
// extern TaskHandle_t wifiTaskHandle;
extern TaskHandle_t webServerTaskHandle;
extern TaskHandle_t cleanupStorageTaskHandle;
extern TaskHandle_t healthMonitorTaskHandle;
extern TaskHandle_t batteryTaskHandle;

extern TaskHandle_t watchdogTaskHandle;

void startTasks();
