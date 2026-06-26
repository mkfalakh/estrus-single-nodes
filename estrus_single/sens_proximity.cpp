#include "esp32-hal-gpio.h"
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

static bool proxActiveLow = true;

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

  uint32_t timeoutDirtySec = sysConfig.dirty_timeout_hours * 3600UL;
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

    unsigned long now = millis();

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

      // standing definition
      bool standing = (s1 || s2);

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

      // ==========================
      // SYSTEM CORE
      // ==========================

      if (rtcValid) {

        // Partition Event
        checkTimeTransitions();

        // Standing Stats
        updateGlobalStats(standing);

        // updatePartitionStats(standing);

        // Evaluate Estrus
        result = evaluateEstrus();
      }

      // ==========================
      // SYSTEM STATE
      // ==========================
      sysSetSensorState(
        s1,
        s2,
        d1,
        d2);

      if (rtcValid) {

        sysSetEstrusResult(result);
      }

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

// SET MODE SENSOR PROXIMITY
void setProximityActiveLow(bool activeLow) {
  proxActiveLow = activeLow;
}

// NORMALIZE LOGIC
static bool normalize(bool raw) {
  return proxActiveLow ? !raw : raw;
}

// READ SENSOR
bool readProx1() {
  return normalize(digitalRead(PROX1_PIN));
}

bool readProx2() {
  return normalize(digitalRead(PROX2_PIN));
}

void initProximity() {

  if (sysConfig.prox_active_low) {
    pinMode(PROX1_PIN, INPUT_PULLUP);
    pinMode(PROX2_PIN, INPUT_PULLUP);
  } else {
    pinMode(PROX1_PIN, INPUT);
    pinMode(PROX2_PIN, INPUT);
  }

  logToFile("🔁 PROX MODE: %s",
            sysConfig.prox_active_low ? "LOW" : "HIGH");
}
