#include "storage_cleanup.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "logger.h"
#include "csv_writer.h"
#include "system_state.h"
#include <SD.h>

// HELPER
static bool isOlderThanRetention(
  const DateTime &fileDate) {

  DateTime now = getNow();

  uint32_t ageDays =
    (now.unixtime() - fileDate.unixtime())
    / 86400UL;

  return (
    ageDays > sysConfig.retention_days);
}

static bool parseDateFromFilename(
  const String &filename,
  DateTime &dt) {

  // minimal:
  // X-YYYY-MM-DD.csv
  if (filename.length() < 14)
    return false;

  String name = filename;

  // hapus extension
  if (name.endsWith(".csv")) {
    name.remove(name.length() - 4);
  } else if (name.endsWith(".txt")) {
    name.remove(name.length() - 4);
  }

  // ambil 10 karakter terakhir
  String dateStr =
    name.substring(name.length() - 10);

  int y, m, d;

  if (
    sscanf(
      dateStr.c_str(),
      "%d-%d-%d",
      &y,
      &m,
      &d)
    != 3) {

    return false;
  }

  dt = DateTime(
    y,
    m,
    d,
    0,
    0,
    0);

  return true;
}

// PROSES CLEANUP FOLDER
static void cleanupFolder(
  const char *path) {

  File dir =
    SD.open(path);

  if (!dir) {
    sysSetSD(false);
    return;
  }

  sysSetSD(true);

  File file;

  while (
    (file = dir.openNextFile())) {

    if (file.isDirectory()) {

      file.close();

      continue;
    }

    String name =
      file.name();

    DateTime fileDate;

    if (

      parseDateFromFilename(
        name,
        fileDate)

      &&

      isOlderThanRetention(
        fileDate)) {

      String fullPath =
        String(file.name());

      if (!fullPath.startsWith("/")) {

        fullPath =
          String(path)
          + "/"
          + fullPath;
      }

      file.close();

      SD.remove(fullPath);

      logToFile(
        "🗑 Deleted: %s",
        fullPath.c_str());

    } else {

      file.close();
    }
  }

  dir.close();
}

// TASK
void cleanupStorageTask(
  void *pv) {

  unsigned long lastRun = 0;

  while (true) {

    if (!SYS.rtc_ok) {

      vTaskDelay(pdMS_TO_TICKS(60000));

      continue;
    }

    if (millis() - lastRun > 86400000UL) {

      lastRun =
        millis();

      if (sdMutex && xSemaphoreTake(sdMutex, pdMS_TO_TICKS(5000))) {

        cleanupFolder("/data");

        cleanupFolder("/log");

        xSemaphoreGive(
          sdMutex);

        logToFile(
          "🧹 Storage cleanup done");
      }
    }

    vTaskDelay(
      pdMS_TO_TICKS(60000));
  }
}
