#include "health_monitor.h"
#include "rtc_manager.h"
#include "ina_manager.h"
#include "sd_manager.h"
#include "system_state.h"
#include "logger.h"

void healthMonitorTask(void *pv) {

  static unsigned long lastRTCDriftCheck = 0;
  static uint32_t lastRTCRecoverTry = 0;
  static uint32_t lastINARecoverTry = 0;

  while (true) {

    // ==================================
    // SD RECOVERY
    // ==================================

    if (!SYS.sd_ok) {

      logToFile(
        "[HEALTH] SD recovery...");

      if (takeSDMutex("HEALTH", pdMS_TO_TICKS(1000))) {

        bool ok = remountSDCard();

        giveSDMutex();

        if (ok) {

          logToFile(
            "[HEALTH] ✅ SD recovered");
        }
      }
    }

    // ==================================
    // RTC RECOVERY
    // ==================================

    bool rtcHealthy = checkRTCHealth();

    if (!rtcHealthy) {

      logToFile("[HEALTH] ❌ RTC lost");

      sysSetRTC(false);

      if (millis() - lastRTCRecoverTry > 10000) {

        lastRTCRecoverTry = millis();

        logToFile(
          "[HEALTH] RTC recovery...");

        initRTC();
      }

    } else {

      sysSetRTC(true);
    }

    // ==================================
    // INA RECOVERY
    // ==================================

    bool inaHealthy = checkINAHealth();

    if (!inaHealthy) {

      logToFile("[HEALTH] ❌ INA226 lost");

      sysSetINA(false);

      if (millis() - lastINARecoverTry > 10000) {

        lastINARecoverTry = millis();

        logToFile(
          "[HEALTH] INA recovery...");

        initINA226();
      }

    } else {

      sysSetINA(true);
    }

    vTaskDelay(
      pdMS_TO_TICKS(5000));
  }
}
