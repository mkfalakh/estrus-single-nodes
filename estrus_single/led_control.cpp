#include "HardwareSerial.h"
#include "led_control.h"
#include "config.h"
#include "system_state.h"
#include "logger.h"

static LedState ledState = LED_NORMAL;
static LedState lastState = LED_NORMAL;

static unsigned long lastBlink = 0;
static int blinkStep = 0;
static bool ledOn = false;

static unsigned long wifiWakeTs = 0;

void initLED() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // ledcAttach(LED_R, LED_FREQ, LED_RES);
  // ledcAttach(LED_G, LED_FREQ, LED_RES);
  // ledcAttach(LED_B, LED_FREQ, LED_RES);

  setLED(0, 0, 0);
  Serial.println("✅ LED Ready");
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
  // invert karena common anode
  // ledcWrite(LED_R, 255 - r);
  // ledcWrite(LED_G, 255 - g);
  // ledcWrite(LED_B, 255 - b);

  // common cathode (GND)
  // ledcWrite(LED_R, r);
  // ledcWrite(LED_G, g);
  // ledcWrite(LED_B, b);

  // use analog PWM (pin adc)
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

void updateLedFromSystem() {

  if (sysIsError()) {

    ledState = LED_ERROR;

  } else if (sysIsSensorDirty()) {

    ledState = LED_DIRTY;

  } else if (sysIsLowBattery()) {

    ledState = LED_LOW_BAT;

  } else if (sysIsAlarm()) {

    ledState = LED_ALARM;

  } else {

    ledState = LED_NORMAL;
  }

  if (sysWifiWakeActive()) {

    wifiWakeTs = millis();
  }
}

void ledTask(void *pv) {

  while (true) {

    updateLedFromSystem();  // baca state LED

    if (millis() - wifiWakeTs < 2000) {

      if ((millis() / 200) % 2) {

        setLED(0, 0, 255);

      } else {

        setLED(0, 0, 0);
      }

      vTaskDelay(
        10 / portTICK_PERIOD_MS);

      continue;
    }

    if (ledState != lastState) {
      blinkStep = 0;
      lastBlink = millis();
      lastState = ledState;
    }

    unsigned long now = millis();

    switch (ledState) {

      // ====================
      // NORMAL (HIJAU NYALA TERUS)
      // ====================
      case LED_NORMAL:
        setLED(0, 255, 0);
        break;

      // ====================
      // SENSOR DIRTY (MERAH NYALA TERUS)
      // ====================
      case LED_DIRTY:
        setLED(255, 0, 0);
        break;

      // ====================
      // LOW BAT (MERAH BLINK TERUS)
      // ====================
      case LED_LOW_BAT:

        if (now - lastBlink > 300) {
          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn)
            setLED(255, 0, 0);
          else
            setLED(0, 0, 0);

          // blinkStep++;

          // if (blinkStep >= 6) {  // 3x
          //   blinkStep = 0;
          //   lastBlink = now + 1000;
          // }
        }

        break;

      // ====================
      // ALARM (BIRU BLINK TERUS)
      // ====================
      case LED_ALARM:

        if (now - lastBlink > 200) {
          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn)
            setLED(0, 0, 255);
          else
            setLED(0, 0, 0);

          blinkStep++;

          if (blinkStep >= 4) {
            blinkStep = 0;
            lastBlink = now + 800;
          }
        }

        break;

      // ====================
      // ERROR INIT SISTEM (KUNING BLINK TERUS)
      // ====================
      case LED_ERROR:

        if (now - lastBlink > 300) {
          lastBlink = now;

          ledOn = !ledOn;

          if (ledOn)
            setLED(170, 255, 0);
          else
            setLED(0, 0, 0);
        }

        break;
    }

    // debug
    // logToFile(
    //   "[LED CHECK] SD:%d RTC:%d SENSOR:%d ERROR:%d\n",
    //   SYS.sd_ok,
    //   SYS.rtc_ok,
    //   SYS.sensor_ok,
    //   sysIsError());

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
