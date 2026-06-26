#include "soc/soc.h"
#include "task_manager.h"
#include "task_monitor.h"
#include "led_control.h"
#include "buzzer.h"
#include "button.h"
#include "csv_writer.h"
#include "logger.h"
#include "wifi_task.h"
#include "sens_proximity.h"
#include "storage_cleanup.h"
#include "web_server.h"
#include "health_monitor.h"
#include "power_monitor.h"

TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t csvTaskHandle = NULL;
TaskHandle_t loggerTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t buzzerTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t webServerTaskHandle = NULL;
TaskHandle_t cleanupStorageTaskHandle = NULL;
TaskHandle_t healthMonitorTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;

TaskHandle_t watchdogTaskHandle = NULL;

// ========================
// START ALL TASKS
// ========================
void startTasks() {

  // =====================================
  // HEALTH SYSTEM MONITORING
  // =====================================
  xTaskCreatePinnedToCore(
    healthMonitorTask,
    "Health",
    8192,
    NULL,
    3,
    &healthMonitorTaskHandle,
    1);

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
  // BATTERY TASK
  // =====================================
  xTaskCreatePinnedToCore(
    batteryTask,
    "Battery",
    4096,
    NULL,
    1,
    &batteryTaskHandle,
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
    4096,
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
    4096,
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
    &buttonTaskHandle,
    1);

  // =====================================
  // WIFI AP
  // =====================================
  xTaskCreatePinnedToCore(
    wifiTask,
    "WiFiTask",
    4096,
    NULL,
    1,
    &wifiTaskHandle,
    0);

  // =====================================
  // WEB SERVER — core 1, prio 1 agar Sensor (prio 2) tidak terpreempt saat
  // handleHistory/handleDownload membaca SD byte-per-byte
  // =====================================
  xTaskCreatePinnedToCore(
    webServerTask,
    "WEB",
    8192,
    nullptr,
    1,
    &webServerTaskHandle,
    1);

  // =====================================
  // CLEANUP STORAGE
  // =====================================
  xTaskCreatePinnedToCore(
    cleanupStorageTask,
    "Cleanup",
    8192,
    NULL,
    1,
    &cleanupStorageTaskHandle,
    1);

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
