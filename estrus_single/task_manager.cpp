#include "task_manager.h"
#include "task_monitor.h"
#include "led_control.h"
#include "buzzer.h"
#include "csv_writer.h"
#include "logger.h"
#include "wifi_task.h"
#include "sens_proximity.h"

TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t csvTaskHandle = NULL;
TaskHandle_t loggerTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

TaskHandle_t watchdogTaskHandle = NULL;

// ========================
// START ALL TASKS
// ========================
void startTasks() {

  // =====================================
  // SENSOR TASK
  // =====================================
  xTaskCreatePinnedToCore(
    sensorTask,
    "Sensor",
    8192,
    NULL,
    2,
    &sensorTaskHandle,
    1);

  // =====================================
  // CSV WRITER
  // =====================================
  xTaskCreatePinnedToCore(
    csvWriterTask,
    "CSVWriter",
    8192,
    NULL,
    1,
    &csvTaskHandle,
    1);

  // =====================================
  // LOGGER
  // =====================================
  xTaskCreatePinnedToCore(
    loggerTask,
    "Logger",
    8192,
    NULL,
    1,
    &loggerTaskHandle,
    1);

  // =====================================
  // LED
  // =====================================
  xTaskCreatePinnedToCore(
    ledTask,
    "LED",
    2048,
    NULL,
    1,
    &ledTaskHandle,
    1);

  // =====================================
  // BUZZER
  // =====================================
  xTaskCreatePinnedToCore(
    buzzerTask,
    "Buzzer",
    2048,
    NULL,
    1,
    &buzzerTaskHandle,
    1);

  // =====================================
  // BUTTON
  // =====================================
  xTaskCreatePinnedToCore(
    buttonTask,
    "Button",
    4096,
    NULL,
    1,
    &buzzerTaskHandle,
    1);

  // =====================================
  // WIFI AP
  // =====================================
  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiTask",
    3072,
    NULL,
    1,
    &wifiTaskHandle,
    0);

  // =====================================
  // WATCHDOG MONITOR
  // =====================================
  xTaskCreatePinnedToCore(
    watchdogTask,
    "Watchdog",
    4096,
    NULL,
    1,
    &watchdogTaskHandle,
    0);

  Serial.println("✅ All Tasks Started");
}
