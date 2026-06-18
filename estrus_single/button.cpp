#include "button.h"
#include "esp32-hal-gpio.h"
#include "buzzer.h"
#include "config.h"
#include "config_runtime.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "wifi_manager.h"
#include "logger.h"
#include <Arduino.h>

void initButton() {
  pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);
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

        buzzerPlay(
          BUZZER_STOP_CONFIRM);
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

    // =========================
    // LONG PRESS
    // =========================
    case BTN_LONG_PRESS:

      Serial.println("⚠️ LONG PRESS");

      if (!sysIsSystemFault()) {

        resetConfig();

        Serial.println("⚠️ Config reset!");

        buzzerPlay(BUZZER_LONG_PRESS);
      }

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

  const unsigned long DEBOUNCE_MS = 100;
  const unsigned long DOUBLE_MS = 700;
  const unsigned long LONG_MS = 8000;

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
  // LONG PRESS
  // =========================

  if (current == LOW && pressStart != 0 && !longHandled && now - pressStart >= LONG_MS) {

    longHandled = true;

    clickCount = 0;

    lastStable = current;

    return BTN_LONG_PRESS;
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

    ButtonEvent event = getButtonEvent();

    if (event != BTN_NONE) {

      handleButtonEvent(event);
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
