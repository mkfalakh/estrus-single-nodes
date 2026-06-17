#include "storage_cleanup.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "system_state.h"
#include "logger.h"
#include "csv_writer.h"
#include "system_state.h"
#include <SD.h>

// HELPER
bool isOlderThanRetention(const DateTime &fileDate) {

  DateTime now = getNow();

  int ageDays =
    (now.unixtime() - fileDate.unixtime()) / 86400;

  return ageDays >= sysConfig.retention_days;
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
  } else if (name.endsWith(".log")) {
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
static void cleanupFolder(const char *path) {

  File dir = SD.open(path);

  if (!dir) {
    sysSetSD(false);

    giveSDMutex();

    return;
  }

  sysSetSD(true);

  constexpr int MAX_DELETE = 64;

  String filesToDelete[MAX_DELETE];
  int deleteCount = 0;

  File file;

  while ((file = dir.openNextFile())) {

    if (file.isDirectory()) {

      file.close();
      continue;
    }

    String name = file.name();

    DateTime fileDate;

    if (parseDateFromFilename(name, fileDate) && isOlderThanRetention(fileDate)) {

      String fullPath = String(file.name());

      if (!fullPath.startsWith("/")) {

        fullPath =
          String(path) + "/" + fullPath;
      }

      if (deleteCount < MAX_DELETE) {

        filesToDelete[deleteCount++] =
          fullPath;
      }
    }

    file.close();

    taskYIELD();
  }

  dir.close();

  for (int i = 0; i < deleteCount; i++) {

    if (SD.remove(filesToDelete[i])) {

      logToFile(
        "🗑 Deleted: %s",
        filesToDelete[i].c_str());

    } else {

      logToFile(
        "⚠️ Failed delete: %s",
        filesToDelete[i].c_str());
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


// TASK
void cleanupStorageTask(void *pv) {

  bool firstRun = true;
  static uint8_t lastCleanupDay = 255;

  while (true) {

    // RTC belum valid → tunggu
    if (!SYS.rtc_ok || !SYS.sd_ok) {

      Serial.println("⚠️ Cleanup Storage paused: RTC invalid or SD invalid");

        vTaskDelay(pdMS_TO_TICKS(30000));

      continue;
    }

    DateTime now = getNow();

    bool needCleanup =
      firstRun || (now.day() != lastCleanupDay && now.hour() == 0 && now.minute() == 0);

    if (needCleanup) {

      firstRun = false;
      lastCleanupDay = now.day();

      if (takeSDMutex("CLEANUP", pdMS_TO_TICKS(5000))) {

        logToFile(
          "🧹 Starting storage cleanup...");

        cleanupFolder("/data");
        cleanupFolder("/log");

        giveSDMutex();

        logToFile(
          "🧹 Storage cleanup done");
      } else {

        logToFile(
          "⚠️ Cleanup skipped: SD busy");
      }
    }

    // cek setiap 1 menit
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}
