#include "HardwareSerial.h"
#include "led_control.h"
#include "config.h"
#include "config_runtime.h"
#include "system_state.h"
#include "logger.h"

volatile LedPattern ledPattern = LED_IDLE;

static unsigned long lastBlink = 0;
static int blinkStep = 0;
static bool ledOn = false;

static unsigned long wifiWakeTs = 0;

// helper
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  // use analog PWM (pin adc)
  // common anode LED RGB
  analogWrite(LED_R, 255 - r);
  analogWrite(LED_G, 225 - g);
  analogWrite(LED_B, 225 - b);
}

inline void ledOff() {
  setLED(0, 0, 0);
}

inline void ledGreen() {
  setLED(0, 180, 0);
}

inline void ledRed() {
  setLED(180, 0, 0);
}

inline void ledBlue() {
  setLED(0, 0, 180);
}

inline void ledYellow() {
  setLED(180, 180, 0);
}

inline void ledOrange() {
  setLED(255, 60, 0);
}

// tes led
// void testLEDColors() {

//   ledRed();
//   delay(2000);

//   ledGreen();
//   delay(2000);

//   ledBlue();
//   delay(2000);

//   ledYellow();
//   delay(2000);

//   ledOrange();
//   delay(2000);

//   ledOff();
// }

// init led
void initLED() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // testLEDColors();
  ledOff();

  Serial.println("✅ LED Ready");
}

// led pattern
void updateLedPattern() {

  if (sysIsSensorDirty()) {

    ledPattern = LED_SENSOR_DIRTY;

  } else if (sysIsSystemFault()) {

    ledPattern = LED_FAULT;

  } else if (
    SYS.estrus && (!SYS.alarm_ack || !sysConfig.stop_after_alarm)) {

    ledPattern = LED_ESTRUS;

  } else if (sysIsLowBattery()) {

    ledPattern = LED_LOW_BATTERY;

  } else {

    ledPattern = LED_IDLE;
  }
}

// LED TASK
void ledTask(void *pv) {

  static LedPattern lastLedPattern = LED_NONE;

  while (true) {

    updateLedPattern();

    // logging transisi led
    if (ledPattern != lastLedPattern) {

      switch (ledPattern) {

        case LED_IDLE:
          logToFile("💡 LED -> IDLE (GREEN)");
          break;

        case LED_ESTRUS:
          logToFile("💡 LED -> ESTRUS (BLUE)");
          break;

        case LED_FAULT:
          logToFile(
            "💡 LED -> FAULT (YELLOW) SD:%d RTC:%d INA:%d",
            SYS.sd_ok,
            SYS.rtc_ok,
            SYS.ina_ok);
          break;

        case LED_SENSOR_DIRTY:
          logToFile(
            "💡 LED -> SENSOR DIRTY (ORANGE) S1:%d S2:%d",
            SYS.sensor1_dirty,
            SYS.sensor2_dirty);
          break;

        case LED_LOW_BATTERY:
          logToFile(
            "💡 LED -> LOW BATTERY (RED) %.2f%%",
            SYS.battery_pct);
          break;

        default:
          break;
      }

      lastLedPattern = ledPattern;
    }

    unsigned long now = millis();

    if (sysIsSensorDirty()) {

      ledPattern = LED_SENSOR_DIRTY;

    } else if (sysIsSystemFault()) {

      ledPattern = LED_FAULT;

    } else if (SYS.estrus && (!SYS.alarm_ack || !sysConfig.stop_after_alarm)) {

      ledPattern = LED_ESTRUS;

    } else if (sysIsLowBattery()) {

      ledPattern = LED_LOW_BATTERY;

    } else {

      ledPattern = LED_IDLE;
    }

    switch (ledPattern) {

      // ====================
      // NORMAL (HIJAU NYALA TERUS)
      // ====================
      case LED_IDLE:

        ledGreen();

        ledOn = false;
        blinkStep = 0;

        break;

      // ====================
      // SENSOR DIRTY (ORANYE)
      // ====================
      case LED_SENSOR_DIRTY:

        if (now - lastBlink > 150) {

          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn) {
            ledOrange();
          } else {
            ledOff();
          }

          blinkStep++;

          if (blinkStep >= 4) {

            blinkStep = 0;

            lastBlink = now + 1500;
          }
        }

        break;

      // ====================
      // LOW BAT (MERAH BLINK TERUS)
      // ====================
      case LED_LOW_BATTERY:

        if (now - lastBlink > 500) {

          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn) {
            ledRed();
          } else {
            ledOff();
          }

          blinkStep++;

          if (blinkStep >= 4) {

            blinkStep = 0;

            lastBlink = now + 3000;
          }
        }

        break;

      // ====================
      // LED ESTRUS (BIRU BLINK TERUS)
      // ====================
      case LED_ESTRUS:

        if (now - lastBlink > 200) {

          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn) {
            ledBlue();
          } else {
            ledOff();
          }

          blinkStep++;

          if (blinkStep >= 4) {

            blinkStep = 0;

            lastBlink = now + 800;
          }
        }

        break;

      // ====================
      // ERROR/FAULT INIT SISTEM (KUNING BLINK TERUS)
      // ====================
      case LED_FAULT:

        if (now - lastBlink > (ledOn ? 300 : 2000)) {

          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn) {
            ledYellow();
          } else {
            ledOff();
          }
        }

        break;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
