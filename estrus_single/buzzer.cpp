#include "esp32-hal-gpio.h"
#include "buzzer.h"
#include "button.h"
#include "config.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "wifi_manager.h"
#include "logger.h"
#include <Arduino.h>

static unsigned long lastToggle = 0;
static bool buzOn = false;
static int step = 0;

unsigned long lastTrigger = 0;

static volatile BuzzerPattern buzzerPattern = BUZZER_NONE;
#define LOW_BATTERY_INTERVAL_MS (30UL * 60UL * 1000UL)

void buzzerOn() {
#if BUZZER_PASSIVE
  ledcWrite(BUZZER_PIN, 128);
#else
  digitalWrite(BUZZER_PIN, HIGH);
#endif
}

void buzzerOff() {
#if BUZZER_PASSIVE
  ledcWrite(BUZZER_PIN, 0);
#else
  digitalWrite(BUZZER_PIN, LOW);
#endif
}

void initBuzzer() {
#if BUZZER_PASSIVE
  ledcAttach(BUZZER_PIN, BUZ_FREQ, BUZ_RES);
#else
  pinMode(BUZZER_PIN, OUTPUT);
#endif
  buzzerOff();
}

void buzzerPlay(BuzzerPattern pattern) {

  buzzerPattern = pattern;
}


// ALARM BUNYI KETIKA MENCAPAI INTERVAL [sistem lama]
// void checkIntervalTrigger() {
//   unsigned long now = millis();

//   unsigned long intervalMs = sysConfig.interval_hours * 3600000UL;

//   if (now - lastTrigger >= intervalMs) {
//     lastTrigger = now;

//     if (sysConfig.alarm_enabled) {
//       sysTriggerAlarm();
//       logToFile("🔔 Interval trigger alarm");
//     }
//   }
// }

// BUZZER TASK
void buzzerTask(void *pv) {

  while (true) {

    unsigned long now = millis();
    static unsigned long lastLowBatteryBeep = 0;

    // ERROR MODE (PRIORITY)
    if (sysIsError()) {

      if (now - lastToggle > 800) {
        lastToggle = now;

        buzOn = !buzOn;

        if (buzOn) buzzerOn();
        else buzzerOff();
      }

      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // LOW BATTERY MODE
    if (sysIsLowBattery() && !sysIsAlarm() && sysConfig.alarm_enabled) {

      if (millis() - lastLowBatteryBeep >= LOW_BATTERY_INTERVAL_MS) {

        buzzerPattern = BUZZER_LOW_BATTERY;

        lastLowBatteryBeep = millis();
      }
    }

    switch (buzzerPattern) {

      case BUZZER_LOW_BATTERY:

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(500));

        buzzerOff();
        vTaskDelay(pdMS_TO_TICKS(500));

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(500));

        buzzerOff();

        buzzerPattern = BUZZER_NONE;

        continue;

      case BUZZER_DOUBLE_CLICK:

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOff();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOff();

        buzzerPattern = BUZZER_NONE;

        continue;

      case BUZZER_STOP_CONFIRM:

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(100));

        buzzerOff();

        buzzerPattern = BUZZER_NONE;

        continue;

      default:
        break;
    }

    // ALARM MODE
    if (sysIsAlarm() && sysConfig.alarm_enabled) {

      if (now - lastToggle > 200) {
        lastToggle = now;

        buzOn = !buzOn;

        if (buzOn) buzzerOn();
        else buzzerOff();

        step++;

        // 2x beep lalu jeda
        if (step >= 4) {
          step = 0;
          lastToggle = now + 800;
        }
      }

      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // IDLE
    buzzerOff();
    step = 0;

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
