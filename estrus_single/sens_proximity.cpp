#include "esp32-hal-gpio.h"
#include "sens_proximity.h"
#include "sens_ina226.h"
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

static uint16_t sensor1StableCount = 0;
static uint16_t sensor2StableCount = 0;

static bool sensor1Dirty = false;
static bool sensor2Dirty = false;

static bool lastSensor1 = false;
static bool lastSensor2 = false;

bool isSensor1Dirty() {
  return sensor1Dirty;
}

bool isSensor2Dirty() {
  return sensor2Dirty;
}

// ========================
// CHECK SENSOR DIRTY
// ========================
void updateDirtyDetection(bool s1, bool s2) {

  // SENSOR 1
  if (s1 == lastSensor1) {

    if (sensor1StableCount < UINT16_MAX) {
      sensor1StableCount++;
    }

  } else {

    sensor1StableCount = 0;

    sensor1Dirty = false;

    lastSensor1 = s1;
  }

  if (sensor1StableCount >= sysConfig.dirty_timeout_samples) {

    sensor1Dirty = true;
  }

  // SENSOR 2
  if (s2 == lastSensor2) {

    if (sensor2StableCount < UINT16_MAX) {
      sensor2StableCount++;
    }

  } else {

    sensor2StableCount = 0;

    sensor2Dirty = false;

    lastSensor2 = s2;
  }

  if (sensor2StableCount >= sysConfig.dirty_timeout_samples) {

    sensor2Dirty = true;
  }
}

// RESET DIRTY DETECTION
void resetDirtyDetection() {

  sensor1Dirty = false;
  sensor2Dirty = false;

  sensor1StableCount = 0;
  sensor2StableCount = 0;
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
  static DateTime dirty1Since;
  static DateTime dirty2Since;

  while (true) {

    unsigned long now = millis();

    if (now - lastSample >= (sysConfig.record_interval_sec * 1000UL)) {

      lastSample = now;

      // ==========================
      // SENSOR STATE
      // ==========================
      bool s1 = readProx1();
      bool s2 = readProx2();

      updateDirtyDetection(s1, s2);

      bool d1 = isSensor1Dirty();
      bool d2 = isSensor2Dirty();

      // ==========================
      // SENSOR 1 DIRTY EVENT
      // ==========================
      if (d1 != lastD1) {

        if (d1) {

          dirty1Since = getNow();

          logToFile(
            "🟠 Sensor 1 dirty detected");

        } else {

          TimeSpan span =
            getNow() - dirty1Since;

          logToFile(
            "🟢 Sensor 1 dirty cleared "
            "(%ld d %ld h %ld m)",

            span.days(),
            span.hours(),
            span.minutes());
        }

        lastD1 = d1;
      }

      // ==========================
      // SENSOR 2 DIRTY EVENT
      // ==========================
      if (d2 != lastD2) {

        if (d2) {

          dirty2Since = getNow();

          logToFile(
            "🟠 Sensor 2 dirty detected");

        } else {

          TimeSpan span =
            getNow() - dirty2Since;

          logToFile(
            "🟢 Sensor 2 dirty cleared "
            "(%ld d %ld h %ld m)",

            span.days(),
            span.hours(),
            span.minutes());
        }

        lastD2 = d2;
      }

      // set sensor dirty
      sysSetSensorDirty(d1 || d2);

      // standing definition
      bool standing = (s1 || s2);

      // ==========================
      // POWER
      // ==========================
      float voltage = readVoltage();
      float current = readCurrent();
      float power = voltage * current;

      float dt =
        (now - lastPowerTs) / 1000.0f;

      lastPowerTs = now;

      updatePowerStats(
        power,
        voltage,
        dt);

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

        rtcValid =
          (t.year() >= 2026);

        if (!rtcValid && millis() - lastRtcLog >= 60000) {

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
        updatePartitionStats(standing);

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

      sysSetSensorHealth(!(d1 && d2));

      sysSetPower(
        powerStats.percentage,
        voltage,
        current,
        power);

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

        data.voltage = voltage;
        data.current = current;
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
          "S:%d/%d D:%d/%d "
          "| Rate:%.1f%% "
          "Base:%.1f%% "
          "Dev:%.1f%% "
          "| Estrus:%d "
          "| V:%.1f%% I:%.1f%% W:%.1f%% Bat:%.1f%%",

          s1,
          s2,

          d1,
          d2,

          result.current_rate * 100.0f,
          result.baseline_rate * 100.0f,

          result.deviation_pct,
          result.estrus,

          voltage,
          current,
          power,
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
