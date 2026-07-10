#include "button.h"
#include "esp32-hal-gpio.h"
#include "buzzer.h"
#include "led_control.h"
#include "config.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "wifi_manager.h"
#include "logger.h"
#include <Arduino.h>

static bool restartHandled = false;
static bool resetHandled = false;

static uint32_t restartPressTs = 0;
static uint32_t resetPressTs = 0;

void initButton() {
  pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESTART_BUTTON_PIN, INPUT_PULLUP);
}

// HELPER
static void handleButtonEvent(ButtonEvent event) {

  switch (event) {

    // =========================
    // SINGLE CLICK
    // =========================
    case BTN_SINGLE_CLICK:

      Serial.println("SINGLE CLICK");

      if (sysIsAlarm()) {

        if (sysConfig.stop_after_alarm) {

          acknowledgeAlarm();

          Serial.println(
            "🔕 Alarm acknowledged by button");

        } else {

          sysStopAlarm();

          Serial.println(
            "🔕 Alarm stopped by button");
        }

        buzzerPlay(BUZZER_STOP_CONFIRM);
      }

      break;

    // =========================
    // DOUBLE CLICK
    // =========================
    case BTN_DOUBLE_CLICK:

      Serial.println("DOUBLE CLICK");

      if (!wifiEnabled) {

        enableWiFiAP();
      }

      lastClientTime = millis();

      sysTriggerWifiWake();

      buzzerPlay(BUZZER_DOUBLE_CLICK);

      Serial.println(
        "📡 WiFi wake by button");

      break;

    default:
      break;
  }
}

ButtonEvent getButtonEvent(bool pressed) {

  static bool lastReading = false;
  static bool stableState = false;
  static bool lastStable = false;

  static uint32_t debounceTs = 0;

  static uint32_t clickStart = 0;

  static uint8_t clickCount = 0;

  const uint32_t DEBOUNCE_MS = 50;
  const uint32_t DOUBLE_MS = 200;

  uint32_t now = millis();

  // =========================
  // DEBOUNCE
  // =========================

  if (pressed != lastReading) {

    debounceTs = now;

    lastReading = pressed;
  }

  if (now - debounceTs >= DEBOUNCE_MS) {

    stableState = pressed;
  }

  bool current = stableState;

  // =========================
  // FALLING EDGE
  // =========================

  if (!lastStable && current) {

    if (clickCount == 0) {

      clickCount = 1;

      clickStart = now;

    } else if (clickCount == 1 && now - clickStart <= DOUBLE_MS) {

      clickCount = 2;
    }
  }

  // =========================
  // DOUBLE CLICK
  // =========================

  if (clickCount == 2) {

    clickCount = 0;

    lastStable = current;

    return BTN_DOUBLE_CLICK;
  }

  // =========================
  // SINGLE CLICK
  // =========================

  if (clickCount == 1 && now - clickStart > DOUBLE_MS) {

    clickCount = 0;

    lastStable = current;

    return BTN_SINGLE_CLICK;
  }

  lastStable = current;

  return BTN_NONE;
}


// BUTTON TASK
void buttonTask(void *pv) {

  vTaskDelay(pdMS_TO_TICKS(1000));

  while (true) {

    uint32_t now = millis();

    int alarmRaw =
      digitalRead(BUZZER_BUTTON_PIN);

    int restartRaw =
      digitalRead(RESTART_BUTTON_PIN);

    bool alarmBtn =
      (alarmRaw == LOW);

    bool restartBtn =
      (restartRaw == LOW);

    // DEBUG BUTTON
    // logToFile(
    //   "RAW a=%d r=%d | BOOL a=%d r=%d",
    //   alarmRaw,
    //   restartRaw,
    //   alarmBtn,
    //   restartBtn);

    // ===========================
    // SINGLE / DOUBLE CLICK
    // ===========================

    if (!restartBtn) {

      ButtonEvent event =
        getButtonEvent(alarmBtn);

      if (event != BTN_NONE) {

        handleButtonEvent(event);
      }
    }

    // ===========================
    // FACTORY RESET
    // ===========================

    if (alarmBtn && restartBtn) {

      logToFile("ENTER FACTORY BLOCK");

      if (resetPressTs == 0) {

        resetPressTs = now;

        restartPressTs = 0;

        restartHandled = false;

        ledPattern = LED_FACTORY_RESET;
      }

      if (!resetHandled && now - resetPressTs >= 5000) {

        logToFile("FACTORY RESET EXECUTE");

        resetHandled = true;

        logToFile("🧹 Factory Reset");

        buzzerPlay(BUZZER_LONG_PRESS);

        resetConfig();

        pendingRestart = true;
      }

    } else {

      resetPressTs = 0;

      resetHandled = false;
    }

    // ===========================
    // RESTART
    // ===========================

    if (restartBtn && !alarmBtn) {

      if (restartPressTs == 0) {

        restartPressTs = now;

        ledPattern = LED_RESTART;
      }

      if (!restartHandled && now - restartPressTs >= 3000) {

        restartHandled = true;

        logToFile("🔄 Restart");

        buzzerPlay(BUZZER_STOP_CONFIRM);

        pendingRestart = true;
      }

    } else {

      restartPressTs = 0;

      restartHandled = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


// TEST BUTTON
// void buttonTask(void *pv) {

//   Serial.println("BUTTON TASK OK");

//   pinMode(
//     BUZZER_BUTTON_PIN,
//     INPUT_PULLUP);

//   while (true) {

//     int s =
//       digitalRead(BUZZER_BUTTON_PIN);

//     Serial.printf(
//       "BTN=%d\n",
//       s);

//     vTaskDelay(
//       500 / portTICK_PERIOD_MS);
//   }
// }
