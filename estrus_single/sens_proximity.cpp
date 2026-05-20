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
#include <Wire.h>
#include <Arduino.h>

static bool proxActiveLow = true;

// ========================
// MODEL STATE
// ========================
typedef struct {
  float baseline;
  int persist;
  int cooldown;
  int prevA;
  float lastScore;
} EstrusModel;

static EstrusModel em = { 0 };

// ========================
// CONFIG
// ========================
float EMA_ALPHA = sysConfig.ema_alpha;
float SCORE_THRESHOLD = sysConfig.score_threshold;
float RATIO_TRIGGER = sysConfig.ratio_trigger;
int PERSIST_REQ = sysConfig.persist_required;

// ========================
// GET SCORE
// ========================
float getLastEstrusScore() {
  return em.lastScore;
}

// ========================
// DETECTOR
// ========================
bool detectEstrusAdvanced(int a1, int a2, int hour) {

  int A = a1 + a2;

  // ========================
  // INIT BASELINE
  // ========================
  if (em.baseline <= 0) {
    em.baseline = A;
  }

  // ========================
  // EMA BASELINE
  // ========================
  em.baseline = (EMA_ALPHA * A) + (1.0f - EMA_ALPHA) * em.baseline;

  float base = max(em.baseline, 1.0f);

  // ========================
  // FEATURE
  // ========================

  // ratio
  float R = A / base;

  // balance
  float B = 0;
  if (max(a1, a2) > 0) {
    B = (float)min(a1, a2) / max(a1, a2);
  }

  // trend
  float T = (A - em.prevA) / base;
  em.prevA = A;

  // persist logic
  if (R > RATIO_TRIGGER) {
    em.persist++;
  } else {
    if (em.persist > 0) em.persist--;
  }

  // time weight
  float W = (hour >= 20 || hour <= 5) ? 1.2f : 1.0f;

  // ========================
  // NORMALISASI
  // ========================
  float Rn = min(R / 3.0f, 1.0f);
  float Bn = B;
  float Tn = constrain(T, 0.0f, 1.0f);
  float Pn = min(em.persist / 5.0f, 1.0f);

  // ========================
  // SCORE
  // ========================
  float score =
    0.35f * Rn + 0.25f * Bn + 0.20f * Pn + 0.15f * Tn + 0.05f * W;

  em.lastScore = score;

  // ========================
  // COOLDOWN
  // ========================
  if (em.cooldown > 0) {
    em.cooldown--;
    return false;
  }

  // ========================
  // DECISION
  // ========================
  if (score > SCORE_THRESHOLD && em.persist >= PERSIST_REQ) {
    em.cooldown = 50;
    em.persist = 0;
    return true;
  }

  // ========================
  // DEBUG
  // ========================
  // logToFile(
  //   "A:%d|%d | T:%d | Score:%.2f | Persist:%d | Baseline:%.2f\n",
  //   a1, a2, A,
  //   score,
  //   em.persist,
  //   em.baseline);

  // logToFile(
  //   "A:%d Score:%.2f Persist:%d Baseline:%.2f\n",
  //   a1 + a2,
  //   score,
  //   em.persist,
  //   em.baseline);

  return false;
}

// ========================
// SENSOR TASK
// ========================
void sensorTask(void *pv) {

  int a1 = 0;
  int a2 = 0;

  unsigned long lastSample = millis();
  unsigned long windowStart = millis();
  unsigned long lastPowerTs = millis();
  unsigned long lastDebug = 0;

  SensorData data;

  while (true) {

    unsigned long now = millis();

    // =====================================
    // SENSOR SAMPLING
    // =====================================
    if (now - lastSample >= SAMPLE_MS) {

      lastSample = now;

      if (readProx1()) a1++;
      if (readProx2()) a2++;
    }

    // =====================================
    // WINDOW COMPLETE
    // =====================================
    if (now - windowStart >= WINDOW_MS) {

      // =====================================
      // TOTAL ACTIVITY
      // =====================================
      int total = a1 + a2;

      // =====================================
      // POWER READ
      // =====================================
      float voltage = readVoltage();
      float current = readCurrent();
      float power = readPower();

      // =====================================
      // POWER STATS UPDATE
      // =====================================
      float dt =
        (now - lastPowerTs) / 1000.0f;

      lastPowerTs = now;

      updatePowerStats(
        power,
        voltage,
        dt);

      // =====================================
      // RTC TIME
      // =====================================
      DateTime t = getNow();

      // =====================================
      // ESTRUS MODEL
      // =====================================
      bool estrus =
        detectEstrusAdvanced(
          a1,
          a2,
          t.hour());

      float score =
        getLastEstrusScore();

      // =====================================
      // UPDATE SYSTEM STATE
      // =====================================
      sysSetActivity(a1, a2);

      sysSetPower(
        powerStats.percentage,
        voltage,
        current,
        power);

      sysSetModel(
        score,
        estrus);

      // =====================================
      // BUILD SENSOR DATA
      // =====================================
      memset(
        &data,
        0,
        sizeof(SensorData));

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

      data.voltage = voltage;
      data.current = current;
      data.power = power;

      data.battery_percent =
        powerStats.percentage;

      data.activity_sensor1 = a1;
      data.activity_sensor2 = a2;
      data.total_activity = total;

      data.score = score;
      data.estrus = estrus;

      // =====================================
      // SEND TO CSV WRITER
      // =====================================
      if (sensorQueue) {

        if (xQueueSend(
              sensorQueue,
              &data,
              0)
            != pdTRUE) {

          Serial.println(
            "⚠️ Sensor Queue Full");
        }
      }

      // =====================================
      // PERIODIC DEBUG
      // =====================================
      if (now - lastDebug >= 5000) {

        lastDebug = now;

        logToFile("=============  DATA  ==============");  //35

        logToFile(
          "🐄 A1:%d A2:%d T:%d Score:%.2f Estrus:%d Bat:%.1f%%",
          a1,
          a2,
          total,
          score,
          estrus,
          powerStats.percentage);
      }

      // =====================================
      // RESET WINDOW
      // =====================================
      a1 = 0;
      a2 = 0;

      windowStart = now;
    }

    // =====================================
    // WATCHDOG SAFE
    // =====================================
    vTaskDelay(
      10 / portTICK_PERIOD_MS);
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
