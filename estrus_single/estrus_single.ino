#include "config.h"
#include "logger.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "web_server.h"
#include "ina_manager.h"
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
#include "storage_stats.h"
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
  createSDMutex();

  if (SYS.sd_ok) {
    setSDReadyForLog(true);
    triggerBaselineRecompute();
  }

  initCSVWriter();

  initINA226();

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
    "Ver:%s | Node:%s | Animal:%s",
    FIRMWARE_VERSION,
    sysConfig.node_id,
    sysConfig.animal_id);


  // run 1x to adjust time RTC | must setting first!
  // adjustRTC();
}

void loop() {

  // handleWebServer();

  cleanupSessions();

  // =========================
  // DEBUG SYSTEM
  // =========================
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 30000) {

    // Serial.println(
    //   "LOGGER STACK: %u",
    //   uxTaskGetStackHighWaterMark(NULL));  // jika hasil < 500 = stack hampir habis

    Serial.println("=========  SYSTEM STATE  ==========\n");

    Serial.printf("ERR: %d | RTCSync: %d | SD: %d | RTC: %d | INA: %d | ESTRUS: %d | BUZZ: %d",
                  sysIsSystemFault(), SYS.rtc_ever_synced, SYS.sd_ok, SYS.rtc_ok, SYS.ina_ok, sysIsEstrus(), sysIsAlarm());

    Serial.println("========== RAM & Storage ==========");

    // checkFreeSD();

    Serial.printf("UsedHeap: %d | FreeHeap: %d | HeapSize: %d\n",
                  ESP.getHeapSize() - ESP.getFreeHeap(), ESP.getFreeHeap(), ESP.getHeapSize());

    Serial.printf("UsedPSRAM: %d | FreePSRAM: %d | PSRAMSize: %d\n",
                  ESP.getPsramSize() - ESP.getFreePsram(), ESP.getFreePsram(), ESP.getPsramSize());

    // Serial.println("===================================");

    lastDebug = millis();
  }

  if (pendingRestart && millis() > restartAt) {

    Serial.println("🔄 Restarting system after change device ID");

    delay(500);

    ESP.restart();
  }

  yield();
}
