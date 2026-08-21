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

const uint32_t MIN_HOLD_MS = 300;  // guard: tekan tidak sengaja diabaikan

volatile int feedbackUntil = 0;
const uint32_t FEEDBACK_MS = 150;           // durasi flash LED indikator
const uint32_t RESTART_CLICK_MAX_MS = 200;  // sama dengan MIN_HOLD_MS, biar tidak tumpang tindih

void triggerButtonFeedback() {
  feedbackUntil = millis() + FEEDBACK_MS;
}

void initButton() {
  pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESTART_BUTTON_PIN, INPUT_PULLUP);
}

// HELPER
static void handleButtonEvent(ButtonEvent event, const char *btnName) {

  switch (event) {

    // =========================
    // SINGLE CLICK
    // =========================
    case BTN_SINGLE_CLICK:

      Serial.printf("%s single click\n", btnName);

      // indikasi tombol berfungsi: buzzer + LED flash, selalu jalan
      triggerButtonFeedback();
      buzzerPlay(BUZZER_STOP_CONFIRM);

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
      }

      break;

    // =========================
    // DOUBLE CLICK
    // =========================
    case BTN_DOUBLE_CLICK:

      // sudah tidak dipakai — dibiarkan tanpa aksi
      Serial.printf("%s double click (ignored)\n", btnName);
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
  const uint32_t DOUBLE_MS = 1500;

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

  static uint32_t restartPressStart = 0;
  static bool restartWasPressed = false;

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
    // ALARM BTN - SINGLE / DOUBLE CLICK
    // ===========================
    if (!restartBtn) {
      ButtonEvent event = getButtonEvent(alarmBtn);
      if (event != BTN_NONE) {
        handleButtonEvent(event, "alarmBtn");
      }
    }

    // ===========================
    // RESTART BTN - DETEKSI KLIK PENDEK (bukan hold)
    // ===========================
    if (!alarmBtn) {

      if (restartBtn && !restartWasPressed) {
        restartPressStart = now;
        restartWasPressed = true;
      }

      if (!restartBtn && restartWasPressed) {

        uint32_t heldFor = now - restartPressStart;
        restartWasPressed = false;

        // hanya dianggap "klik" kalau dilepas SEBELUM masuk zona hold-action
        if (heldFor < RESTART_CLICK_MAX_MS) {
          handleButtonEvent(BTN_SINGLE_CLICK, "restartBtn");
        }
      }

    } else {
      restartWasPressed = false;
    }

    // ===========================
    // FACTORY RESET
    // ===========================

    if (alarmBtn && restartBtn) {

      if (resetPressTs == 0) {

        resetPressTs = now;

        restartPressTs = 0;

        restartHandled = false;
      }

      if (now - resetPressTs >= MIN_HOLD_MS) {
        Serial.println("ENTER FACTORY BLOCK");
        ledPattern = LED_FACTORY_RESET;
      }

      if (!resetHandled && now - resetPressTs >= 5000) {

        logToFile("FACTORY RESET EXECUTE");

        resetHandled = true;

        logToFile("🧹 Device Factory Reset");

        buzzerPlay(BUZZER_LONG_PRESS);

        resetConfig();

        pendingRestart = true;
      }

    } else {

      resetPressTs = 0;
      resetHandled = false;
      if (ledPattern == LED_FACTORY_RESET) ledPattern = LED_NONE;
    }

    // ===========================
    // RESTART
    // ===========================

    if (restartBtn && !alarmBtn) {

      if (restartPressTs == 0) {

        restartPressTs = now;
      }

      if (now - restartPressTs >= MIN_HOLD_MS) {
        Serial.println("ENTER RESTART BLOCK");
        ledPattern = LED_RESTART;
      }

      if (!restartHandled && now - restartPressTs >= 3000) {

        restartHandled = true;

        logToFile("🔄 Device Restart");

        buzzerPlay(BUZZER_STOP_CONFIRM);

        pendingRestart = true;
      }

    } else {

      restartPressTs = 0;
      restartHandled = false;
      if (ledPattern == LED_RESTART) ledPattern = LED_NONE;
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
