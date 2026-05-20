#include "csv_writer.h"
#include "logger.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "config_runtime.h"
#include <SD.h>

// ========================
// CONFIG
// ========================
#define CSV_QUEUE_SIZE 50
#define CSV_BATCH_SIZE 10
#define CSV_FLUSH_INTERVAL 5000

// ========================
// GLOBAL
// ========================
QueueHandle_t sensorQueue;

// ========================
// CSV PATH
// ========================
String getCSVPath() {

  String path = "/data/";

  path += sysConfig.node_id;
  path += "-";
  path += todayDateStr();
  path += ".csv";

  return path;
}

// ========================
// INIT
// ========================
void initCSVWriter() {

  // ========================
  // CREATE MUTEX FIRST
  // ========================
  sdMutex = xSemaphoreCreateMutex();

  if (!sdMutex) {

    Serial.println("❌ SD Mutex gagal");

    return;
  }

  // ========================
  // CREATE SENSOR QUEUE
  // ========================
  sensorQueue = xQueueCreate(
    CSV_QUEUE_SIZE,
    sizeof(SensorData));

  if (!sensorQueue) {
    logToFile("❌ Sensor Queue gagal");
    return;
  }

  // ========================
  // START TASK (pindah di task_manager.h)
  // ========================
  // xTaskCreatePinnedToCore(
  //   csvWriterTask,
  //   "CSVWriter",
  //   8192, // safe: 12288
  //   NULL,
  //   1,
  //   NULL,
  //   1);

  logToFile("✅ CSV Writer Ready");
}

// ========================
// CSV TASK
// ========================
void csvWriterTask(void *pv) {

  if (!sdMutex) {

    Serial.println("❌ csvWriterTask no mutex");

    vTaskDelete(NULL);

    return;
  }

  SensorData buffer[CSV_BATCH_SIZE];

  int count = 0;

  unsigned long lastFlush = millis();

  while (true) {

    SensorData incoming;

    // ========================
    // RECEIVE DATA
    // ========================
    if (xQueueReceive(
          sensorQueue,
          &incoming,
          100 / portTICK_PERIOD_MS)) {

      buffer[count++] = incoming;

      // batch full
      if (count >= CSV_BATCH_SIZE) {

        if (sdMutex && xSemaphoreTake(sdMutex, pdMS_TO_TICKS(200))) {

          File f = SD.open(
            getCSVPath(),
            FILE_APPEND);

          if (f) {

            for (int i = 0; i < count; i++) {

              SensorData &d = buffer[i];

              f.printf(
                "%s,%s,%d,%d,%d,%.2f,%d\n",

                sysConfig.node_id,
                d.timestamp,

                d.activity_sensor1,
                d.activity_sensor2,
                d.total_activity,

                d.score,
                d.estrus

              );
            }

            f.close();
          }

          xSemaphoreGive(sdMutex);
        }

        count = 0;
      }
    }

    // ========================
    // PERIODIC FLUSH
    // ========================
    if (millis() - lastFlush > CSV_FLUSH_INTERVAL) {

      lastFlush = millis();

      if (count > 0) {

        if (sdMutex && xSemaphoreTake(sdMutex, pdMS_TO_TICKS(200))) {

          File f = SD.open(
            getCSVPath(),
            FILE_APPEND);

          if (f) {

            for (int i = 0; i < count; i++) {

              SensorData &d = buffer[i];

              f.printf(
                "%s,%s,%d,%d,%d,%.2f,%d\n",

                sysConfig.node_id,
                d.timestamp,

                d.activity_sensor1,
                d.activity_sensor2,
                d.total_activity,

                d.score,
                d.estrus

              );
            }

            f.close();
          }

          xSemaphoreGive(sdMutex);
        }

        count = 0;
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
