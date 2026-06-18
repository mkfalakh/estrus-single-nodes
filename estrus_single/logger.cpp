#include "projdefs.h"
#include "logger.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "config.h"
#include "system_state.h"
#include "config_runtime.h"
#include <stdarg.h>
#include <SD.h>

#define LOG_QUEUE_SIZE 50
#define LOG_LINE_SIZE 256
#define LOG_BATCH_SIZE 5
#define LOG_FLUSH_INTERVAL 5000

SemaphoreHandle_t sdMutex = NULL;
QueueHandle_t logQueue;
bool sdReadyForLog = false;

// ========================
// TYPES
// ========================
typedef struct {
  char text[LOG_LINE_SIZE];
} LogMessage;

// ========================
// HELPERS
// ========================
static String safeNowStr() {

  if (!SYS.rtc_ok)
    return "NO_RTC";

  return nowStr();
}

static String getLogPath() {

  return "/log/" + todayDateStr() + ".log";
}

// ========================
// INIT
// ========================
void initLogger() {

  // ========================
  // CREATE LOG QUEUE
  // ========================
  logQueue = xQueueCreate(
    LOG_QUEUE_SIZE,
    sizeof(LogMessage));

  if (!logQueue) {

    Serial.println("❌ Logger Queue gagal");

    return;
  }

  Serial.println("✅ Logger Ready");
}

// ========================
// SD READY
// ========================
void setSDReadyForLog(bool ready) {
  sdReadyForLog = ready;
}

// ========================
// MAIN LOG API
// ========================
void logToFile(const char *fmt, ...) {

  char msg[LOG_LINE_SIZE];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  String line = "[" + safeNowStr() + "] ";
  line += msg;

  // realtime serial
  Serial.println(line);

  // queue to logger
  if (!logQueue) return;

  LogMessage item;

  strncpy(item.text, line.c_str(), sizeof(item.text));

  xQueueSend(logQueue, &item, 0);
}

// combine dengan logToFile diatas
void logToFile(String msg) {
  logToFile("%s", msg.c_str());
}

// ========================
// LOGGER TASK
// ========================
void loggerTask(void *pv) {

  static LogMessage buffer[LOG_BATCH_SIZE];

  int count = 0;

  unsigned long lastFlush = millis();

  while (true) {

    if (!SYS.sd_ok || !SYS.rtc_ok) {

      Serial.println("⚠️ Loggger paused: RTC invalid or SD invalid");

      vTaskDelay(pdMS_TO_TICKS(5000));

      continue;
    }

    LogMessage incoming;

    // ========================
    // RECEIVE LOG
    // ========================
    if (xQueueReceive(logQueue, &incoming, 100 / portTICK_PERIOD_MS)) {

      buffer[count++] = incoming;

      // batch full
      if (count >= LOG_BATCH_SIZE) {

        if (sdReadyForLog) {

          if (!takeSDMutex("LOGGER", pdMS_TO_TICKS(500))) {

            continue;
          }

          File f = SD.open(getLogPath(), FILE_APPEND);

          if (f) {

            String filename = getLogPath();

            for (int i = 0; i < count; i++) {
              f.println(buffer[i].text);
            }

            f.close();

            giveSDMutex();
          }
        }

        count = 0;
      }
    }

    // ========================
    // PERIODIC FLUSH
    // ========================
    if (millis() - lastFlush > LOG_FLUSH_INTERVAL) {

      lastFlush = millis();

      if (count > 0 && sdReadyForLog) {

        if (sdRecoveredAt != 0 && millis() - sdRecoveredAt < 3000) {

          continue;
        }

        if (!takeSDMutex("LOGGER", pdMS_TO_TICKS(500))) {

          continue;
        }

        File f = SD.open(getLogPath(), FILE_APPEND);

        if (f) {

          // String filename = getLogPath();

          for (int i = 0; i < count; i++) {
            f.println(buffer[i].text);
          }

          f.close();
        }

        giveSDMutex();

        count = 0;
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
