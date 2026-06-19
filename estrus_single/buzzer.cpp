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

// manage alarm estrus & alarm system
bool isFaultAlarm() {

  return !SYS.sd_ok || !SYS.rtc_ok || !SYS.ina_ok || SYS.sensor1_dirty || SYS.sensor2_dirty;
}

bool shouldAlarm() {

  bool estrusAlarm =
    SYS.estrus && sysConfig.alarm_enabled && (!SYS.alarm_ack || !sysConfig.stop_after_alarm);

  bool faultAlarm =
    isFaultAlarm() && sysConfig.alarm_enabled && !SYS.fault_alarm_muted;

  return estrusAlarm || faultAlarm;
}

void acknowledgeAlarm() {

  if (SYS.estrus) {

    SYS.alarm_ack = true;
    logToFile("🔕 Estrus terdeteksi. Alarm acknowledged");
  }

  if (isFaultAlarm()) {

    SYS.fault_alarm_muted = true;
    logToFile("🔕 Fault terdeteksi. Alarm fault muted");
  }

  sysSetAlarm(false);

  buzzerPattern = BUZZER_STOP_CONFIRM;
}

// BUZZER TASK
void buzzerTask(void *pv) {

  static bool lastFaultAlarm = false;
  static bool lastEstrusAlarm = false;
  static bool faultBeeping = false;

  while (true) {

    unsigned long now = millis();
    static unsigned long lastLowBatteryBeep = 0;

    // reset mute fault jika fault sudah hilang
    if (!isFaultAlarm() && SYS.fault_alarm_muted) {

      SYS.fault_alarm_muted = false;

      logToFile("🔔 Fault Alarm mute reset");
    }

    // reset mute estrus jika estrus selesai
    if (!SYS.estrus && SYS.alarm_ack) {

      SYS.alarm_ack = false;

      logToFile("🔔 Estrus Alarm ACK reset");
    }

    // update status alarm global
    sysSetAlarm(shouldAlarm());

    bool estrusAlarm =
      SYS.estrus && sysConfig.alarm_enabled && (!SYS.alarm_ack || !sysConfig.stop_after_alarm);

    bool faultAlarm =
      isFaultAlarm() && sysConfig.alarm_enabled && !SYS.fault_alarm_muted;

    // Log hanya saat alarm mulai berbunyi
    if (faultAlarm && !lastFaultAlarm) {

      logToFile(
        "🚨 Fault alarm active (SD:%d RTC:%d INA:%d S1:%d S2:%d)",
        SYS.sd_ok,
        SYS.rtc_ok,
        SYS.ina_ok,
        SYS.sensor1_dirty,
        SYS.sensor2_dirty);
    }

    // Log saat fault selesai
    if (!faultAlarm && lastFaultAlarm) {

      logToFile(
        "✅ Fault alarm cleared");
    }

    // Log hanya saat estrus mulai berbunyi
    if (estrusAlarm && !lastEstrusAlarm) {

      logToFile(
        "🐄 Estrus alarm active");
    }

    // Log saat estrus selesai
    if (!estrusAlarm && lastEstrusAlarm) {

      logToFile(
        "✅ Estrus alarm cleared");
    }

    lastFaultAlarm = faultAlarm;
    lastEstrusAlarm = estrusAlarm;

    // SYSTEM ERROR MODE (PRIORITY)
    if (faultAlarm) {

      if (now - lastToggle > (faultBeeping ? 300 : 2000)) {

        lastToggle = now;

        faultBeeping = !faultBeeping;

        if (faultBeeping) {
          buzzerOn();
        } else {
          buzzerOff();
        }
      }

      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // LOW BATTERY MODE
    if (sysIsLowBattery()
        && !sysIsAlarm()
        && sysConfig.alarm_enabled) {

      if (now - lastLowBatteryBeep >= LOW_BATTERY_INTERVAL_MS) {

        buzzerPattern = BUZZER_LOW_BATTERY;

        lastLowBatteryBeep = now;
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

      case BUZZER_LONG_PRESS:

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOff();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOn();
        vTaskDelay(pdMS_TO_TICKS(80));

        buzzerOff();

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

    // ESTRUS MODE
    if (estrusAlarm) {

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
