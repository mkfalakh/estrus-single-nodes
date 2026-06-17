#include "csv_writer.h"
#include "logger.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "system_state.h"
#include "config_runtime.h"
#include "sensor_data.h"
#include "storage_stats.h"
#include <SD.h>

// ========================
// CONFIG
// ========================
#define CSV_QUEUE_SIZE 200
#define CSV_BATCH_SIZE 10
#define CSV_FLUSH_INTERVAL 60000UL  // 1 menit

// ========================
// GLOBAL
// ========================
QueueHandle_t sensorQueue;

// ========================
// Helper
// ========================
static String getCsvPath() {

  return "/data/" + todayDateStr() + ".csv";
}

static void writeHeaderIfNeeded(File &f) {

  if (f.size() == 0) {

    f.println(
      "device_id,"
      "animal_id,"
      "timestamp,"
      "sensor1_state,"
      "sensor2_state,"
      "sensor1_dirty,"
      "sensor2_dirty,"
      "deviation,"
      "estrus,"
      "voltage,"
      "current,"
      "battery_pct");
  }
}

static bool flushCsvBuffer(SensorData *buffer, int &count) {

  if (count <= 0) return true;

  if (!SYS.sd_ok) {
    return false;
  }

  // tunggu SD stabil setelah remount sdcard
  if (sdRecoveredAt != 0 && millis() - sdRecoveredAt < 3000) {

    return false;
  }

  if (!takeSDMutex("CSV", pdMS_TO_TICKS(3000))) {

    logToFile("⚠️ CSV: mutex timeout");
    return false;
  }

  bool success = false;

  int maxWrite = min(count, 10);

  File f = SD.open(
    getCsvPath(),
    FILE_APPEND);

  if (f) {

    String filename = getCsvPath();

    writeHeaderIfNeeded(f);

    for (int i = 0; i < maxWrite; i++) {

      SensorData &d = buffer[i];

      f.printf(
        "%s,%s,%s,%d,%d,%d,%d,%.2f,%d,%.2f,%.2f,%.2f\n",

        sysConfig.node_id,
        sysConfig.animal_id,

        d.timestamp,

        d.sensor1_state,
        d.sensor2_state,
        d.sensor1_dirty,
        d.sensor2_dirty,

        d.deviation_pct,
        d.estrus,

        d.voltage,
        d.current,
        d.battery_pct);
    }

    csvRowsWritten += maxWrite;

    f.close();

    success = true;

  } else {

    sysSetSD(false);

    logToFile(
      "❌ CSV open failed: %s",
      getCsvPath().c_str());
  }

  giveSDMutex();

  if (!success) {

    return false;
  }

  if (count > maxWrite) {

    memmove(
      buffer,
      buffer + maxWrite,
      (count - maxWrite) * sizeof(SensorData));
  }

  count -= maxWrite;

  sysSetSD(true);

  return true;
}

// ========================
// INIT
// ========================
void initCSVWriter() {

  if (!SD.exists("/data")) {

    SD.mkdir("/data");
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

  logToFile("✅ CSV Writer Ready");
}

// ========================
// CSV TASK
// ========================
void csvWriterTask(void *pv) {

  // lebih aman jika batch dibesarkan nanti
  static SensorData buffer[CSV_BATCH_SIZE];

  int count = 0;

  unsigned long lastFlush =
    millis();

  uint32_t rowsWritten = 0;

  static uint8_t lastDay = 0;
  static bool csvPaused = false;

  while (true) {

    // skip write csv if RTC invalid/broken
    if (!SYS.rtc_ok || !SYS.sd_ok) {

      if (!csvPaused) {
        csvPaused = true;

        logToFile("CSV paused: RTC invalid or SD invalid");
      }

      vTaskDelay(pdMS_TO_TICKS(5000));

      continue;
    }

    csvPaused = false;

    DateTime now = getNow();

    if (lastDay == 255) {

      lastDay = now.day();

    } else if (lastDay != now.day()) {

      csvRowsWritten = 0;

      lastDay = now.day();

      logToFile(
        "📄 CSV counter reset");
    }

    SensorData incoming;

    // ========================
    // RECEIVE DATA
    // ========================

    if (xQueueReceive(
          sensorQueue,
          &incoming,
          pdMS_TO_TICKS(100))
        == pdTRUE) {

      // flush dulu jika penuh
      if (SYS.sd_ok && count >= CSV_BATCH_SIZE) {

        int oldCount = count;

        if (flushCsvBuffer(buffer, count)) {

          rowsWritten += oldCount;

          lastFlush = millis();
        }
      }

      buffer[count++] = incoming;

      // ========================
      // PERIODIC FLUSH
      // ========================

      if (SYS.sd_ok && count > 0 && millis() - lastFlush >= CSV_FLUSH_INTERVAL) {

        int oldCount = count;

        if (flushCsvBuffer(buffer, count)) {

          rowsWritten += oldCount;

          lastFlush = millis();
        }
      }

      // ========================
      // DEBUG
      // ========================

      static unsigned long lastStat = 0;

      if (millis() - lastStat > 60000) {

        logToFile(
          "📄 CSV rows=%lu queue=%u",

          rowsWritten,

          uxQueueMessagesWaiting(
            sensorQueue));

        lastStat =
          millis();
      }

      vTaskDelay(
        pdMS_TO_TICKS(10));
    }
  }
}
