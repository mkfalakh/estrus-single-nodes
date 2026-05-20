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

static unsigned long lastToggle = 0;
static bool buzOn = false;
static int step = 0;

unsigned long lastTrigger = 0;

static unsigned long wifiBeepTs = 0;

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

void initBuzzerAndBtn() {
#if BUZZER_PASSIVE
  ledcAttach(BUZZER_PIN, BUZ_FREQ, BUZ_RES);
#else
  pinMode(BUZZER_PIN, OUTPUT);
#endif

  pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);
  buzzerOff();
}

bool buttonPressed() {

  static bool lastState = HIGH;

  static unsigned long lastDebounce = 0;

  bool reading =
    digitalRead(BUZZER_BUTTON_PIN);

  // tombol berubah
  if (reading != lastState) {

    lastDebounce = millis();
  }

  // debounce stabil
  if (
    millis() - lastDebounce > BUTTON_DEBOUNCE_MS) {

    // active LOW
    if (
      lastState == HIGH && reading == LOW) {

      lastState = reading;

      return true;
    }
  }

  lastState = reading;

  return false;
}

// ALARM BUNYI KETIKA MENCAPAI INTERVAL
void checkIntervalTrigger() {
  unsigned long now = millis();

  unsigned long intervalMs = sysConfig.interval_hours * 3600000UL;

  if (now - lastTrigger >= intervalMs) {
    lastTrigger = now;

    if (sysConfig.buzzer_enabled) {
      sysTriggerAlarm();
      logToFile("🔔 Interval trigger alarm");
    }
  }
}

// BUZZER TASK
void buzzerTask(void *pv) {

  while (true) {

    unsigned long now = millis();

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

    // WIFI AP ON
    if (sysWifiWakeActive()) {

      wifiBeepTs = millis();
    }

    if (millis() - wifiBeepTs < 500) {

      buzzerOn();

      vTaskDelay(80 / portTICK_PERIOD_MS);

      buzzerOff();

      vTaskDelay(80 / portTICK_PERIOD_MS);

      buzzerOn();

      vTaskDelay(80 / portTICK_PERIOD_MS);

      buzzerOff();

      continue;
    }

    // ALARM MODE
    if (sysIsAlarm()) {

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

// BUTTON TASK
void buttonTask(void *pv) {

  bool lastButton = HIGH;

  unsigned long pressTime = 0;

  uint8_t clickCount = 0;

  const unsigned long DOUBLE_MS = 700;

  while (true) {

    bool current =
      digitalRead(BUZZER_BUTTON_PIN);

    // =====================================
    // FALLING EDGE
    // =====================================
    if (
      lastButton == HIGH && current == LOW) {

      Serial.println(
        "BUTTON PRESS");

      unsigned long now = millis();

      // click pertama
      if (clickCount == 0) {

        clickCount = 1;

        pressTime = now;
      }

      // click kedua
      else if (clickCount == 1) {

        if (
          now - pressTime < DOUBLE_MS) {

          clickCount = 2;
        }

        else {

          clickCount = 1;

          pressTime = now;
        }
      }

      // debounce press
      vTaskDelay(
        50 / portTICK_PERIOD_MS);
    }

    // =====================================
    // PROCESS CLICK
    // =====================================
    if (clickCount > 0) {

      unsigned long now = millis();

      if (
        now - pressTime > DOUBLE_MS) {

        // ===============================
        // SINGLE CLICK
        // ===============================
        if (clickCount == 1) {

          Serial.println(
            "SINGLE CLICK");

          if (sysIsAlarm()) {

            sysStopAlarm();

            logToFile(
              "🔕 Alarm stopped by button");
          }
        }

        // ===============================
        // DOUBLE CLICK
        // ===============================
        else if (clickCount == 2) {

          Serial.println(
            "DOUBLE CLICK");

          enableWiFiAP();

          lastClientTime = millis();

          sysTriggerWifiWake();

          logToFile(
            "📡 WiFi wake by button");
        }

        clickCount = 0;
      }
    }

    lastButton = current;

    vTaskDelay(
      10 / portTICK_PERIOD_MS);
  }
}


// BEFORE
// void buttonTask(void *pv) {

//   static unsigned long firstClickTs = 0;

//   static uint8_t clickCount = 0;

//   const unsigned long DOUBLE_CLICK_MS = 350;

//   while (true) {

//     // =====================================
//     // BUTTON PRESSED
//     // =====================================
//     if (buttonPressed()) {

//       unsigned long now = millis();

//       // click pertama
//       if (clickCount == 0) {
//         Serial.println("BUTTON 1x");

//         clickCount = 1;

//         firstClickTs = now;
//       }

//       // click kedua
//       else if (clickCount == 1) {
//         Serial.println("DOUBLE CLICK!");

//         // within timeout
//         if (
//           now - firstClickTs <= DOUBLE_CLICK_MS) {

//           clickCount = 2;
//         }

//         // timeout lewat -> reset
//         else {

//           clickCount = 1;

//           firstClickTs = now;
//         }
//       }
//     }

//     // =====================================
//     // PROCESS CLICK
//     // =====================================
//     if (clickCount > 0) {

//       unsigned long now = millis();

//       // timeout selesai
//       if (
//         now - firstClickTs > DOUBLE_CLICK_MS) {

//         // ===============================
//         // SINGLE CLICK
//         // ===============================
//         if (clickCount == 1) {

//           if (sysIsAlarm()) {

//             sysStopAlarm();

//             logToFile(
//               "🔕 Alarm stopped by button");
//           }
//         }

//         // ===============================
//         // DOUBLE CLICK
//         // ===============================
//         else if (clickCount == 2) {

//           enableWiFiAP();

//           lastClientTime = millis();

//           sysTriggerWifiWake();

//           logToFile(
//             "📡 WiFi wake by button");
//         }

//         // reset
//         clickCount = 0;
//       }
//     }

//     vTaskDelay(
//       20 / portTICK_PERIOD_MS);
//   }
// }


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
