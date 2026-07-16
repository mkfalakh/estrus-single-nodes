#include "esp32-hal-gpio.h"
#include "driver/gpio.h"
#include "sens_proximity.h"
#include "ina_manager.h"
#include "config.h"
#include "config_runtime.h"
#include "logger.h"
#include "csv_writer.h"
#include "power_monitor.h"
#include "rtc_manager.h"
#include "system_state.h"
#include "sensor_data.h"
#include "estrus_model.h"
#include "storage_stats.h"
#include <Wire.h>
#include <Arduino.h>

// Debounce window for mechanical proximity switch edges.
// Hardware fix already applied; this is firmware-side insurance.
#define PROX_DEBOUNCE_MS 15

static volatile bool proxActiveLow = true;

// ========================
// INTERRUPT STATE
// Hardware-driven detection. ISR updates volatile state on every
// pin edge (CHANGE), so the main loop never misses fast transitions
// regardless of its workload.
// ========================
static volatile bool s1Active = false;
static volatile bool s2Active = false;
static volatile bool s1Changed = false;
static volatile bool s2Changed = false;
static volatile uint32_t s1LastEdgeMs = 0;
static volatile uint32_t s2LastEdgeMs = 0;
static portMUX_TYPE proxMux = portMUX_INITIALIZER_UNLOCKED;
static bool proxIntAttached = false;

static uint32_t sensor1ActiveSince = 0;
static uint32_t sensor2ActiveSince = 0;
static uint32_t sensor1InactiveSince = 0;
static uint32_t sensor2InactiveSince = 0;

static bool lastSensor1 = false;
static bool lastSensor2 = false;

// ========================
// CHECK SENSOR DIRTY
// ========================
void updateDirtyDetection(bool s1, bool s2) {

  uint32_t timeoutDirtySec = sysConfig.dirty_timeout_min * 60UL;
  uint32_t timeoutActivitySec = sysConfig.no_activity_timeout_hours * 3600UL;

  // SENSOR 1
  if (s1) {

    sensor1ActiveSince += sysConfig.record_interval_sec;

    sensor1InactiveSince = 0;
    SYS.sensor1_no_activity = false;

    SYS.sensor1_dirty = (sensor1ActiveSince >= timeoutDirtySec);

  } else {

    sensor1ActiveSince = 0;
    SYS.sensor1_dirty = false;

    sensor1InactiveSince += sysConfig.record_interval_sec;
    SYS.sensor1_no_activity = (sensor1InactiveSince >= timeoutActivitySec);
  }

  // SENSOR 2
  if (s2) {

    sensor2ActiveSince += sysConfig.record_interval_sec;

    sensor2InactiveSince = 0;
    SYS.sensor2_no_activity = false;

    SYS.sensor2_dirty = (sensor2ActiveSince >= timeoutDirtySec);

  } else {

    sensor2ActiveSince = 0;
    SYS.sensor2_dirty = false;

    sensor2InactiveSince += sysConfig.record_interval_sec;
    SYS.sensor2_no_activity = (sensor2InactiveSince >= timeoutActivitySec);
  }

  logToFile(
    "DIRTY S1=%lu/%lu S2=%lu/%lu | NO ACTIVITY S1=%lu/%lu S2=%lu/%lu",

    sensor1ActiveSince,
    timeoutDirtySec,
    sensor2ActiveSince,
    timeoutDirtySec,

    sensor1InactiveSince,
    timeoutActivitySec,
    sensor2InactiveSince,
    timeoutActivitySec);
}

// RESET DIRTY DETECTION
void resetDirtyDetection() {

  SYS.sensor1_dirty = false;
  SYS.sensor2_dirty = false;

  sensor1ActiveSince = 0;
  sensor2ActiveSince = 0;

  sensor1InactiveSince = 0;
  sensor2InactiveSince = 0;

  SYS.sensor1_no_activity = false;
  SYS.sensor2_no_activity = false;
}

// FAST EDGE CONSUMPTION
// Called from sensorTask on every 100 ms loop so fast HIGH/LOW
// transitions are visible immediately, independent of the slower
// CSV record gate.  Returns true on state change and provides the
// age (ms) since the last accepted edge.
static bool consumeProx1Change(bool& active, uint32_t& ageMs) {
  bool changed = false;
  portENTER_CRITICAL(&proxMux);
  active = s1Active;
  ageMs = millis() - s1LastEdgeMs;   // overflow-safe
  changed = s1Changed;
  s1Changed = false;
  portEXIT_CRITICAL(&proxMux);
  return changed;
}

static bool consumeProx2Change(bool& active, uint32_t& ageMs) {
  bool changed = false;
  portENTER_CRITICAL(&proxMux);
  active = s2Active;
  ageMs = millis() - s2LastEdgeMs;   // overflow-safe
  changed = s2Changed;
  s2Changed = false;
  portEXIT_CRITICAL(&proxMux);
  return changed;
}

// ========================
// SENSOR TASK
// ========================
void sensorTask(void *pv) {

  static unsigned long lastDebug = 0;
  static unsigned long lastRtcLog = 0;
  unsigned long lastSample = 0;
  unsigned long lastPowerTs = millis();
  static bool lastEstrus = false;

  // cek sensor kotor
  static bool lastD1 = false;
  static bool lastD2 = false;
  static bool lastNA1 = false;
  static bool lastNA2 = false;
  static DateTime dirty1Since;
  static DateTime dirty2Since;

  while (true) {

    static unsigned long lastEnergySave = 0;
    static unsigned long lastSerialState = 0;

    unsigned long now = millis();

    // ==========================
    // FAST EDGE DETECTION
    // Consume ISR change flags every 100 ms so fast HIGH/LOW
    // transitions are visible immediately, independent of the
    // slower CSV record gate below.
    // Serial output only — NO SD writes on the fast path to
    // avoid SD wear and blocking I/O on a 100 ms loop.
    // ==========================
    bool s1Fast, s2Fast;
    uint32_t s1Age, s2Age;
    bool s1Trans = consumeProx1Change(s1Fast, s1Age);
    bool s2Trans = consumeProx2Change(s2Fast, s2Age);

    if (s1Trans) {
      Serial.printf("⚡ PROX1 %s (stable %lums)\n",
                    s1Fast ? "ACTIVE" : "INACTIVE", s1Age);
    }
    if (s2Trans) {
      Serial.printf("⚡ PROX2 %s (stable %lums)\n",
                    s2Fast ? "ACTIVE" : "INACTIVE", s2Age);
    }

    // ==========================
    // REAL-TIME SYS STATE SYNC
    // Push ISR-captured sensor state to SYS immediately so the
    // dashboard (5 s poll) and alarm logic see changes within
    // ~100 ms instead of waiting for the record_interval_sec gate.
    // Record samples (CSV) stay decoupled — they are for offline
    // folder evaluation only, not for the running system state.
    // ==========================
    if (s1Trans || s2Trans) {
      SYS.sensor1 = s1Fast;
      SYS.sensor2 = s2Fast;
    }

    // Periodic live reading print every 5 s so the current state
    // can be checked via Arduino IDE serial monitor even with no
    // transitions. Matches the dashboard API poll cadence.
    if (now - lastSerialState >= 5000) {
      Serial.printf("[SENSOR LIVE] S1:%d S2:%d\n",
                    SYS.sensor1 ? 1 : 0,
                    SYS.sensor2 ? 1 : 0);
      lastSerialState = now;
    }

    if (now - lastSample >= (sysConfig.record_interval_sec * 1000UL)) {

      lastSample = now;

      // ==========================
      // SENSOR STATE
      // ==========================
      bool s1 = readProx1();
      bool s2 = readProx2();

      // cek perubahan state sensor
      Serial.printf("[SENSOR STATE] s1: %d | s2: %d\n", s1, s2);

      updateDirtyDetection(s1, s2);

      bool d1 = SYS.sensor1_dirty;
      bool d2 = SYS.sensor2_dirty;

      // ==========================
      // SENSOR 1 DIRTY EVENT
      // ==========================
      if (d1 != lastD1) {

        if (d1) {

          if (SYS.rtc_ok) {
            dirty1Since = getNow();
          }

          logToFile(
            "🟣 Sensor 1 dirty detected");

        } else {

          if (SYS.rtc_ok) {

            TimeSpan span =
              getNow() - dirty1Since;

            logToFile(
              "🟢 Sensor 1 dirty cleared "
              "(%ld d %ld h %ld m)",

              span.days(),
              span.hours(),
              span.minutes());

          } else {

            logToFile(
              "🟢 Sensor 1 dirty cleared");
          }
        }

        lastD1 = d1;
      }

      // ==========================
      // SENSOR 2 DIRTY EVENT
      // ==========================
      if (d2 != lastD2) {

        if (d2) {

          if (SYS.rtc_ok) {
            dirty2Since = getNow();
          }

          logToFile(
            "🟣 Sensor 2 dirty detected");

        } else {

          if (SYS.rtc_ok) {

            TimeSpan span =
              getNow() - dirty2Since;

            logToFile(
              "🟢 Sensor 2 dirty cleared "
              "(%ld d %ld h %ld m)",

              span.days(),
              span.hours(),
              span.minutes());

          } else {

            logToFile(
              "🟢 Sensor 2 dirty cleared");
          }
        }

        lastD2 = d2;
      }

      // ==========================
      // SENSOR NO ACTIVITY EVENT
      // ==========================
      bool na1 = SYS.sensor1_no_activity;
      bool na2 = SYS.sensor2_no_activity;

      if (na1 != lastNA1) {

        if (na1) {

          logToFile(
            "🟠 Sensor 1 no activity");

        } else {

          logToFile(
            "🟢 Sensor 1 activity restored");
        }

        lastNA1 = na1;
      }

      if (na2 != lastNA2) {

        if (na2) {

          logToFile(
            "🟠 Sensor 2 no activity");

        } else {

          logToFile(
            "🟢 Sensor 2 activity restored");
        }

        lastNA2 = na2;
      }

      // ==========================
      // ESTRUS RESULT
      // ==========================
      EstrusResult result;

      memset(
        &result,
        0,
        sizeof(result));

      // ==========================
      // RTC CHECK
      // ==========================
      bool rtcValid = false;

      DateTime t;

      if (SYS.rtc_ok) {

        t = getNow();

        rtcValid = (t.year() >= 2026);

        if (!rtcValid && millis() - lastRtcLog >= 60000) {

          SYS.rtc_ok = false;

          logToFile(
            "⚠️ RTC year invalid: %d",
            t.year());

          lastRtcLog = millis();
        }
      }

      // reset reading csv rows if new day
      resetCsvRowsIfNewDay();

      // ==========================
      // SYSTEM CORE
      // ==========================

      if (rtcValid) {
        checkTimeTransitions();

        updateSensor2(s2, d1, d2);

        result = evaluateEstrus();

        sysSetEstrusResult(result);
      }

      // ==========================
      // SYSTEM STATE
      // ==========================
      sysSetSensorState(s1, s2, d1, d2);

      // ALARM HANDLING JIKA MENCAPAI ESTRUS
      // validasi estrus jika sesuai baseline samples
      bool currentEstrus = result.valid && result.estrus;

      bool estrusRising = (!lastEstrus && currentEstrus);

      lastEstrus = currentEstrus;

      if (rtcValid && estrusRising && sysConfig.alarm_enabled && !sysIsAlarm() && !(sysConfig.stop_after_alarm && isAlarmAcknowledged())) {

        sysStartAlarm();
      }

      // ==========================
      // BUILD CSV DATA
      // ==========================
      if (rtcValid) {
        SensorData data;

        memset(
          &data,
          0,
          sizeof(data));

        snprintf(
          data.timestamp,
          sizeof(data.timestamp),
          "%04d-%02d-%02d %02d:%02d:%02d",
          t.year(),
          t.month(),
          t.day(),
          t.hour(),
          t.minute(),
          t.second());

        data.sensor1_state = s1;
        data.sensor2_state = s2;
        data.sensor1_dirty = d1;
        data.sensor2_dirty = d2;

        data.deviation_pct = result.deviation_pct;
        data.estrus = result.estrus;

        data.voltage = SYS.battery_voltage;
        data.current = SYS.current;
        data.battery_pct = powerStats.percentage;

        // ==========================
        // QUEUE CSV
        // ==========================
        if (xQueueSend(
              sensorQueue,
              &data,
              pdMS_TO_TICKS(100))
            != pdTRUE) {

          logToFile(
            "⚠️ sensorQueue full");
        }
      }

      // ==========================
      // DEBUG
      // ==========================
      if (millis() - lastDebug >= 60000) {
        logToFile(
          "S1:%d S2:%d D1:%d D2:%d "
          "| Rate:%.1f%% "
          "Base:%.1f%% "
          "Dev:%.1f%% "
          "| Estrus:%d "
          "| V:%.2f%% I:%.2f%% W:%.2f%% Bat:%.0f%%",

          s1,
          s2,

          d1,
          d2,

          result.current_rate * 100.0f,
          result.baseline_rate * 100.0f,

          result.deviation_pct,
          result.estrus,

          SYS.battery_voltage,
          SYS.current,
          SYS.power,
          powerStats.percentage);

        lastDebug = millis();
      }
    }

    vTaskDelay(
      pdMS_TO_TICKS(100));
  }
}

// ========================
// ISR - PROXIMITY SENSORS
// Fires on CHANGE edge. Reads pin level directly (digitalRead is
// safe in IRAM on ESP32), applies active-low/high normalization and
// stores the result in a volatile so the main loop always sees the
// real current state without polling the pin.
// ========================
void IRAM_ATTR prox1ISR() {
  uint32_t now = millis();
  bool raw = (gpio_get_level(GPIO_NUM_7) == 0);  // PROX1_PIN = 7
  bool active = proxActiveLow ? raw : !raw;

  portENTER_CRITICAL(&proxMux);
  if ((now - s1LastEdgeMs) < PROX_DEBOUNCE_MS) {
    portEXIT_CRITICAL(&proxMux);
    return;
  }
  if (active != s1Active) {
    s1Active = active;
    s1Changed = true;
    s1LastEdgeMs = now;
  }
  portEXIT_CRITICAL(&proxMux);
}

void IRAM_ATTR prox2ISR() {
  uint32_t now = millis();
  bool raw = (gpio_get_level(GPIO_NUM_15) == 0);  // PROX2_PIN = 15
  bool active = proxActiveLow ? raw : !raw;

  portENTER_CRITICAL(&proxMux);
  if ((now - s2LastEdgeMs) < PROX_DEBOUNCE_MS) {
    portEXIT_CRITICAL(&proxMux);
    return;
  }
  if (active != s2Active) {
    s2Active = active;
    s2Changed = true;
    s2LastEdgeMs = now;
  }
  portEXIT_CRITICAL(&proxMux);
}

// SET MODE SENSOR PROXIMITY
// Updates active-low/high mapping and re-attaches interrupts so the
// ISR normalization follows the new mode at runtime.
void setProximityActiveLow(bool activeLow) {
  proxActiveLow = activeLow;

  // if interrupts already attached, refresh so ISR uses new polarity
  if (proxIntAttached) {
    detachInterrupt(PROX1_PIN);
    detachInterrupt(PROX2_PIN);
    attachInterrupt(PROX1_PIN, prox1ISR, CHANGE);
    attachInterrupt(PROX2_PIN, prox2ISR, CHANGE);

    // seed volatile state from the actual pin level after re-attach
    bool raw1 = (digitalRead(PROX1_PIN) == LOW);
    bool raw2 = (digitalRead(PROX2_PIN) == LOW);
    portENTER_CRITICAL(&proxMux);
    s1Active = proxActiveLow ? raw1 : !raw1;
    s2Active = proxActiveLow ? raw2 : !raw2;
    s1Changed = false;
    s2Changed = false;
    s1LastEdgeMs = millis();
    s2LastEdgeMs = millis();
    portEXIT_CRITICAL(&proxMux);
  }
}

// READ SENSOR
// Returns the interrupt-captured state. No digitalRead() in the main
// loop anymore — the hardware already told us the current level.
bool readProx1() {
  bool v;
  portENTER_CRITICAL(&proxMux);
  v = s1Active;
  portEXIT_CRITICAL(&proxMux);
  return v;
}

bool readProx2() {
  bool v;
  portENTER_CRITICAL(&proxMux);
  v = s2Active;
  portEXIT_CRITICAL(&proxMux);
  return v;
}

void initProximity() {

  if (sysConfig.prox_active_low) {
    pinMode(PROX1_PIN, INPUT_PULLUP);
    pinMode(PROX2_PIN, INPUT_PULLUP);
  } else {
    pinMode(PROX1_PIN, INPUT);
    pinMode(PROX2_PIN, INPUT);
  }

  // seed initial state from the actual pin level before attaching
  bool raw1 = (digitalRead(PROX1_PIN) == LOW);
  bool raw2 = (digitalRead(PROX2_PIN) == LOW);
  portENTER_CRITICAL(&proxMux);
  s1Active = proxActiveLow ? raw1 : !raw1;
  s2Active = proxActiveLow ? raw2 : !raw2;
  s1Changed = false;
  s2Changed = false;
  s1LastEdgeMs = millis();
  s2LastEdgeMs = millis();
  portEXIT_CRITICAL(&proxMux);

  // attach hardware interrupts — detection burden moves to hardware
  attachInterrupt(PROX1_PIN, prox1ISR, CHANGE);
  attachInterrupt(PROX2_PIN, prox2ISR, CHANGE);
  proxIntAttached = true;

  logToFile("🔁 PROX MODE: %s (INT)",
            sysConfig.prox_active_low ? "LOW" : "HIGH");
}
