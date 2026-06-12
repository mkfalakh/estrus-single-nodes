#include "config.h"
#include "logger.h"
#include "rtc_manager.h"
#include "web_server.h"
#include "sens_ina226.h"
#include "wifi_manager.h"
#include "auth.h"
#include "crypto.h"
#include "sens_proximity.h"
#include "csv_reverse.h"
#include "config_runtime.h"
#include "buzzer.h"
#include "power_monitor.h"
#include "led_control.h"
#include "system_state.h"
#include "csv_writer.h"
#include "task_manager.h"
#include <SPI.h>
#include <Preferences.h>

void setup() {
  Serial.begin(115200);
  delay(200);

  // jangan pakai logToFile sebelum initSDCard()
  Serial.println("System Booting...");

  // =========================
  // LOAD CONFIG
  // =========================
  loadConfig();

  // =========================
  // INIT PIN HARDWARE
  // =========================
  initLED();
  initBuzzer();
  initRTC();  // sysSetRTC()

  initLogger();

  initSDCard();  // di sini set sysSetSD()

  if (SYS.sd_ok) {
    setSDReadyForLog(true);
  }

  initCSVWriter();

  initINA226();  // sysSetSensor() // comment ini jika buat trial atau tidak ada modulnya

  initProximity();  // pinMode sensor
  setProximityActiveLow(sysConfig.prox_active_low);

  // =========================
  // INIT NETWORK & WEB SERVER
  // =========================
  initWiFi();
  initWebServer();
  delay(500);

  // =========================
  // START ALL TASK
  // =========================
  startTasks();

  // =========================
  // FINAL LOG
  // =========================
  logToFile("🚀 System Ready");
  logToFile(
    "Node:%s Animal:%s",
    sysConfig.node_id,
    sysConfig.animal_id);
}

void loop() {

  handleWebServer();
  cleanupSessions();

  // =========================
  // DEBUG SYSTEM
  // =========================
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 60000) {

    // logToFile(
    //   "LOGGER STACK: %u",
    //   uxTaskGetStackHighWaterMark(NULL));  // jika hasil < 500 = stack hampir habis

    logToFile("=========  SYSTEM STATE  ==========");

    logToFile("ERR: %d | SD: %d | RTC: %d | SENSOR: %d | ESTRUS: %d | BUZZ: %d",
              sysIsError(), SYS.sd_ok, SYS.rtc_ok, SYS.sensor_ok, sysIsEstrus(), sysIsAlarm());

    logToFile("========== RAM & Storage ==========");

    checkFreeSD();

    logToFile("UsedHeap: %d | FreeHeap: %d | HeapSize: %d",
              ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getFreeHeap(), ESP.getHeapSize());

    logToFile("UsedPSRAM: %d | FreePSRAM: %d | PSRAMSize: %d",
              ESP.getPsramSize() - ESP.getFreePsram(), ESP.getFreePsram(), ESP.getPsramSize());

    // logToFile("===================================");

    lastDebug = millis();
  }

  if (pendingRestart && millis() > restartAt) {

    logToFile("🔄 Restarting system after change device ID");

    delay(500);

    ESP.restart();
  }
}
