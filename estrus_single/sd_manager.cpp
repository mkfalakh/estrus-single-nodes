#include "sd_manager.h"
#include "logger.h"
#include "rtc_manager.h"
#include "config.h"
#include "system_state.h"
#include "config_runtime.h"
#include <Arduino.h>
#include <stdarg.h>
#include <SD.h>
#include <SPI.h>

static SPIClass spiSD(FSPI);  // 🔥 ESP32-S3 pakai FSPI

static const char* sdMutexOwner = nullptr;

// helper
bool takeSDMutex(const char* owner, TickType_t timeout) {

  if (!sdMutex) {
    Serial.println("[SDMUTEX] NULL");
    return false;
  }

  bool ok =
    xSemaphoreTake(sdMutex, timeout);

  if (ok) {

    sdMutexOwner = owner;

    Serial.printf(
      "[SDMUTEX] TAKE OK (%s)\n",
      owner);

  } else {

    Serial.printf(
      "[SDMUTEX] TAKE TIMEOUT (%s), owner=%s\n",
      owner,
      (sdMutexOwner && strlen(sdMutexOwner))
        ? sdMutexOwner
        : "UNKNOWN");
  }

  return ok;
}

void giveSDMutex() {

  if (!sdMutex) {
    return;
  }

  if (xSemaphoreGetMutexHolder(sdMutex)
      != xTaskGetCurrentTaskHandle()) {

    Serial.printf(
      "[SDMUTEX] INVALID GIVE by %s\n",
      pcTaskGetName(nullptr));

    return;
  }

  Serial.printf(
    "[SDMUTEX] GIVE (%s)\n",
    sdMutexOwner ? sdMutexOwner : "UNKNOWN");

  xSemaphoreGive(sdMutex);

  sdMutexOwner = nullptr;
}

void createSDMutex() {
  sdMutex = xSemaphoreCreateMutex();

  if (!sdMutex) {

    Serial.println(
      "❌ SD Mutex create failed");

    while (true) {
      delay(1000);
    }
  }
}

// init sdcard
bool initSDCard() {
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("❌ SDCard init gagal!");

    sysSetSD(false);

    // giveSDMutex();

    return false;
  }

  if (!SD.exists("/log")) SD.mkdir("/log");

  Serial.println("✅ SDCard OK");
  sysSetSD(true);
  return true;
}

// cek penyimpanan sdcard
void checkFreeSD() {
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();

  logToFile("SDCard used: %llu | Total: %llu", used, total);
}

// cek jika sdcard terpasang kembali
bool remountSDCard() {

  Serial.println("💾 Remount SD...");

  SD.end();

  spiSD.end();

  delay(100);

  spiSD.begin(
    SD_SCK,
    SD_MISO,
    SD_MOSI,
    SD_CS);

  delay(200);

  for (int i = 0; i < 5; i++) {

    if (SD.begin(SD_CS, spiSD)) {

      uint8_t type = SD.cardType();

      Serial.printf(
        "✅ SD OK type=%d\n",
        type);

      if (!SD.exists("/data")) {
        SD.mkdir("/data");
      }

      if (!SD.exists("/log")) {
        SD.mkdir("/log");
      }

      sysSetSD(true);

      sdRecoveredAt = millis();

      Serial.println("💾 SD recovered");

      return true;
    }

    Serial.printf(
      "❌ Retry remount %d\n",
      i + 1);

    delay(1000);
  }

  sysSetSD(false);

  return false;
}
