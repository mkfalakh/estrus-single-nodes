#include "task_monitor.h"
#include "task_manager.h"
#include "logger.h"
#include <Arduino.h>

// ========================
// PRINT TASK STACK
// ========================
static void printTask(
  const char* name,
  TaskHandle_t handle) {

  if (!handle)
    return;

  UBaseType_t stack =
    uxTaskGetStackHighWaterMark(handle);

  Serial.printf(
    "[TASK] %-10s Stack:%u\n",
    name,
    stack);

  // warning low stack
  if (stack < 300) {

    logToFile(
      "⚠️ LOW STACK: %s = %u",
      name,
      stack);
  }
}

// ========================
// WATCHDOG TASK
// ========================
void watchdogTask(void* pv) {

  while (true) {

    Serial.println(
      "\n===== TASK MONITOR =====");

    printTask(
      "Sensor",
      sensorTaskHandle);

    printTask(
      "CSV",
      csvTaskHandle);

    printTask(
      "Logger",
      loggerTaskHandle);

    printTask(
      "LED",
      ledTaskHandle);

    printTask(
      "Buzzer",
      buzzerTaskHandle);

    printTask(
      "Button",
      buttonTaskHandle);
    
    printTask(
      "wifiTask",
      wifiTaskHandle);

    printTask(
      "Cleanup",
      cleanupStorageTaskHandle);

    // Serial.printf(
    //   "Heap:%u | PSRAM:%u\n",
    //   ESP.getFreeHeap(),
    //   ESP.getFreePsram());

    Serial.println(
      "========================\n");

    vTaskDelay(
      10000 / portTICK_PERIOD_MS);
  }
}
