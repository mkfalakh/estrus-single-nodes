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

#define BUTTON_DEBOUNCE_MS 50

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

      buzzerPlay(
        BUZZER_DOUBLE_CLICK);

      Serial.println(
        "📡 WiFi wake by button");

      break;

    // =========================
    // LONG PRESS
    // =========================
    case BTN_LONG_PRESS:

      Serial.println(
        "⚠️ LONG PRESS");

      Serial.println(
        "⚠️ Long press detected");

      // reserved:
      // factory reset nanti

      break;

    default:
      break;
  }
}

ButtonEvent getButtonEvent() {

  static bool lastButton = HIGH;

  static unsigned long pressStart = 0;

  static unsigned long clickStart = 0;

  static uint8_t clickCount = 0;

  static bool longHandled = false;

  const unsigned long DOUBLE_MS = 700;

  const unsigned long LONG_MS = 5000;

  bool current =
    digitalRead(BUZZER_BUTTON_PIN);

  unsigned long now =
    millis();

  // =========================
  // FALLING EDGE
  // =========================
  if (lastButton == HIGH && current == LOW) {

    pressStart = now;

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
  if (current == LOW && !longHandled && now - pressStart >= LONG_MS) {

    longHandled = true;

    clickCount = 0;

    lastButton = current;

    return BTN_LONG_PRESS;
  }

  // =========================
  // RELEASE
  // =========================
  if (lastButton == LOW && current == HIGH) {

    longHandled = false;
  }

  // =========================
  // DOUBLE CLICK
  // =========================
  if (clickCount == 2) {

    clickCount = 0;

    lastButton = current;

    return BTN_DOUBLE_CLICK;
  }

  // =========================
  // SINGLE CLICK TIMEOUT
  // =========================
  if (clickCount == 1 && now - clickStart > DOUBLE_MS) {

    clickCount = 0;

    lastButton = current;

    return BTN_SINGLE_CLICK;
  }

  lastButton = current;

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
