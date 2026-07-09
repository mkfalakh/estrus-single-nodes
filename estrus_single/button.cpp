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

ButtonEvent getButtonEvent() {

  static bool lastReading = HIGH;
  static bool stableState = HIGH;
  static bool lastStable = HIGH;

  static unsigned long debounceTs = 0;

  static unsigned long pressStart = 0;
  static unsigned long clickStart = 0;

  static uint8_t clickCount = 0;

  static bool longHandled = false;

  const unsigned long DEBOUNCE_MS = 50;
  const unsigned long DOUBLE_MS = 200;

  unsigned long now = millis();

  // =========================
  // DEBOUNCE
  // =========================

  bool reading =
    digitalRead(BUZZER_BUTTON_PIN);

  if (reading != lastReading) {

    debounceTs = now;

    lastReading = reading;
  }

  if (now - debounceTs >= DEBOUNCE_MS) {

    stableState = reading;
  }

  bool current = stableState;

  // =========================
  // FALLING EDGE
  // =========================

  if (lastStable == HIGH && current == LOW) {

    pressStart = now;

    longHandled = false;

    if (clickCount == 0) {

      clickCount = 1;

      clickStart = now;

    } else if (clickCount == 1 && now - clickStart <= DOUBLE_MS) {

      clickCount = 2;
    }
  }

  // =========================
  // RELEASE
  // =========================

  if (lastStable == LOW && current == HIGH) {

    pressStart = 0;

    longHandled = false;
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

  while (true) {

    ButtonEvent event =
      getButtonEvent();

    if (event != BTN_NONE) {

      handleButtonEvent(event);
    }

    bool alarmBtn =
      digitalRead(BUZZER_BUTTON_PIN) == LOW;

    bool restartBtn =
      digitalRead(RESTART_BUTTON_PIN) == LOW;

    uint32_t now =
      millis();

    // =====================================
    // FACTORY RESET
    // =====================================

    if (alarmBtn && restartBtn) {

      if (resetPressTs == 0) {

        resetPressTs = now;

        restartPressTs = 0;

        restartHandled = false;

        ledPattern = LED_FACTORY_RESET;
      }

      if (!resetHandled && now - resetPressTs >= 5000) {

        resetHandled = true;

        logToFile(
          "🧹 Factory Reset");

        buzzerPlay(
          BUZZER_LONG_PRESS);

        resetConfig();

        pendingRestart = true;
      }

    } else {

      resetPressTs = 0;

      resetHandled = false;
    }

    // =====================================
    // RESTART ESP
    // =====================================

    if (restartBtn && !alarmBtn) {

      if (restartPressTs == 0) {

        restartPressTs = now;

        ledPattern = LED_RESTART;
      }

      if (!restartHandled && now - restartPressTs >= 3000) {

        restartHandled = true;

        logToFile(
          "🔄 Restart by button");

        buzzerPlay(
          BUZZER_STOP_CONFIRM);

        pendingRestart = true;
      }

    } else {

      restartPressTs = 0;

      restartHandled = false;
    }

    vTaskDelay(
      pdMS_TO_TICKS(10));
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
