#include "logger.h"
#include "rtc_manager.h"
#include "config.h"
#include "system_state.h"
#include "config_runtime.h"
#include <stdarg.h>
#include <SD.h>
#include <SPI.h>

static SPIClass spiSD(FSPI);  // 🔥 ESP32-S3 pakai FSPI
SemaphoreHandle_t sdMutex = NULL;

bool initSDCard() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("❌ SDCard init gagal!");
    sysSetSD(false);
    return false;
  }

  if (!SD.exists("/log")) SD.mkdir("/log");

  Serial.println("✅ SDCard OK");
  sysSetSD(true);
  return true;
}

void checkFreeSD() {
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();

  logToFile("SDCard used: %llu | Total: %llu", used, total);
}

void logToFile(String msg) {
  logToFile("%s", msg.c_str());
}

// ========================
// CONFIG
// ========================
#define LOG_QUEUE_SIZE 50
#define LOG_LINE_SIZE 256
#define LOG_BATCH_SIZE 5

#define LOG_FLUSH_INTERVAL 3000

// ========================
// TYPES
// ========================
typedef struct {
  char text[LOG_LINE_SIZE];
} LogMessage;

// ========================
// GLOBAL
// ========================
QueueHandle_t logQueue;

bool sdReadyForLog = false;

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
  // CREATE MUTEX FIRST
  // ========================
  sdMutex = xSemaphoreCreateMutex();

  if (!sdMutex) {

    Serial.println("❌ SD Mutex gagal");

    return;
  }

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

// ========================
// LOGGER TASK
// ========================
void loggerTask(void *pv) {

  if (!sdMutex) {

    Serial.println("❌ loggerTask no mutex");

    vTaskDelete(NULL);

    return;
  }

  static LogMessage buffer[LOG_BATCH_SIZE];

  int count = 0;

  unsigned long lastFlush = millis();

  while (true) {

    LogMessage incoming;

    // ========================
    // RECEIVE LOG
    // ========================
    if (xQueueReceive(logQueue, &incoming, 100 / portTICK_PERIOD_MS)) {

      buffer[count++] = incoming;

      // batch full
      if (count >= LOG_BATCH_SIZE) {

        if (sdReadyForLog) {

          File f = SD.open(getLogPath(), FILE_APPEND);

          if (f) {

            String filename = getLogPath();

            for (int i = 0; i < count; i++) {
              f.println(buffer[i].text);
            }

            f.close();

            // update metadata sdcard
            // updateFileTimestamp(filename.c_str());
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

        File f = SD.open(getLogPath(), FILE_APPEND);

        if (f) {

          String filename = getLogPath();

          for (int i = 0; i < count; i++) {
            f.println(buffer[i].text);
          }

          f.close();

          // update metadata sdcard
          // updateFileTimestamp(filename.c_str());
        }

        count = 0;
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}