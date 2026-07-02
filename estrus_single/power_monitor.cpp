#include "power_monitor.h"
#include "rtc_manager.h"
#include "ina_manager.h"
#include "config_runtime.h"
#include "system_state.h"
#include "config.h"
#include "logger.h"

PowerStats powerStats = {
  .energy_mWh = 0,   // energy mWh | total konsumsi batre | default: 0
  .avgPower_mW = 0,  // avg power mW | rata-rata power | default: 0

  // 2x 18650 3350mAh paralel
  // kapasitas = 6700mAh
  // energi nominal = 3.7V × 6700mAh = 24790mWh
  .battery_capacity_mWh = 24790,
  .remaining_mWh = 24790,     // remaining mWh | default: samakan dengan value kapasitas batre mWh
  .percentage = 100,          // persentase batre | default: 100
  .estimated_hours_left = 0,  // estimasi habis batre dalam beberapa jam | default: 0

  .estimated_days_left = 0,
};

static float filteredBatteryVoltage = 0.0f;

// hasil = real voltase baterai / ADC | ganti nilai ini jika voltase baterai tidak sesuai
static constexpr float BATTERY_CORRECTION = 0.949f;


float voltageToPercent(float v) {

  static const float lutV[] = {
    4.20, 4.15, 4.10, 4.05, 4.00, 3.95, 3.90, 3.85, 3.80, 3.75, 3.70, 3.65, 3.60, 3.55, 3.50, 3.40, 3.30
  };

  static const float lutP[] = {
    100, 98, 95, 90, 85, 76, 67, 58, 48, 38, 28, 20, 14, 9, 6, 3, 0
  };

  constexpr int N = sizeof(lutV) / sizeof(lutV[0]);

  if (v >= lutV[0])
    return 100.0f;

  if (v <= lutV[N - 1])
    return 0.0f;

  for (int i = 0; i < N - 1; i++) {

    if (v <= lutV[i] && v >= lutV[i + 1]) {

      float ratio =
        (v - lutV[i + 1]) / (lutV[i] - lutV[i + 1]);

      return lutP[i + 1] + ratio * (lutP[i] - lutP[i + 1]);
    }
  }

  return 0.0f;
}


// update power stats
void updatePowerStats(
  float power_mW,
  float voltage,
  float dt) {

  static float lastPct = 100.0f;

  // ==========================
  // Battery Percentage (Voltage)
  // ==========================

  float newPct =
    voltageToPercent(voltage);

  // update jika berubah >= 2%
  if (fabs(newPct - lastPct) >= 2.0f) {

    lastPct = newPct;
  }

  powerStats.percentage =
    constrain(
      lastPct,
      0.0f,
      100.0f);

  // ==========================
  // Energy Counter
  // ==========================

  float energy =
    power_mW * (dt / 3600.0f);

  powerStats.energy_mWh += energy;

  if (powerStats.energy_mWh < 0.0f) {

    powerStats.energy_mWh = 0.0f;
  }

  if (powerStats.energy_mWh > powerStats.battery_capacity_mWh) {

    powerStats.energy_mWh =
      powerStats.battery_capacity_mWh;
  }

  // ==========================
  // Moving Average Power
  // ==========================

  if (powerStats.avgPower_mW <= 0.1f) {

    // sample pertama
    powerStats.avgPower_mW =
      power_mW;

  } else {

    powerStats.avgPower_mW =
      (powerStats.avgPower_mW * 0.95f)
      + (power_mW * 0.05f);
  }

  // ==========================
  // Remaining Energy
  // ==========================

  float remainByVoltage =
    powerStats.battery_capacity_mWh
    * (powerStats.percentage / 100.0f);

  float remainByCounter =
    powerStats.battery_capacity_mWh
    - powerStats.energy_mWh;

  if (remainByCounter < 0.0f) {

    remainByCounter = 0.0f;
  }

  // gunakan nilai yang lebih kecil
  // agar estimasi tidak terlalu optimistis

  powerStats.remaining_mWh =
    min(
      remainByVoltage,
      remainByCounter);

  // ==========================
  // Runtime Prediction
  // ==========================

  if (powerStats.avgPower_mW > 1.0f) {

    powerStats.estimated_hours_left =
      powerStats.remaining_mWh
      / powerStats.avgPower_mW;

  } else {

    powerStats.estimated_hours_left = 0.0f;
  }

  powerStats.estimated_days_left =
    powerStats.estimated_hours_left
    / 24.0f;
}

// menggunakan divider 4k7 2 buah
float readBatteryVoltageADC() {

  uint32_t sum = 0;

  for (int i = 0; i < 16; i++) {

    sum += analogRead(DIVIDER_PIN);
  }

  float raw =
    sum / 16.0f;

  float adcVoltage =
    (raw / 4095.0f) * 3.3f;

  float batteryVoltage =
    adcVoltage * 2.0f;

  batteryVoltage *=
    BATTERY_CORRECTION;

  // logToFile(
  //   "RAW=%.0f ADC=%.3f BAT=%.3f",
  //   raw,
  //   adcVoltage,
  //   batteryVoltage);

  if (filteredBatteryVoltage == 0.0f) {

    filteredBatteryVoltage =
      batteryVoltage;

  } else {

    filteredBatteryVoltage =
      (filteredBatteryVoltage * 0.95f)
      + (batteryVoltage * 0.05f);
  }

  return filteredBatteryVoltage;
}

// init
void initPowerMonitor() {
  pinMode(DIVIDER_PIN, INPUT);
  analogReadResolution(12);

  analogSetPinAttenuation(
    DIVIDER_PIN,
    ADC_11db);

  logToFile("✅ Battery ADC OK");
}

// untuk prediksi habis baterai dalam beberapa hari
void updateBatteryPredictionDate() {

  DateTime now = getNow();

  int days = (int)powerStats.estimated_days_left;

  DateTime future = now + TimeSpan(days * 86400);

  snprintf(powerStats.estimated_date,
           sizeof(powerStats.estimated_date),
           "%04d-%02d-%02d",
           future.year(),
           future.month(),
           future.day());
}


// BATTERY TASK
void batteryTask(void *pv) {

  static unsigned long lastPrediction = 0;
  static unsigned long lastSave = 0;

  unsigned long lastSample = millis();

  while (true) {

    unsigned long now = millis();

    float dt = (now - lastSample) / 1000.0f;

    lastSample = now;

    float batteryVoltage = readBatteryVoltageADC();

    float current = readCurrent();

    float power = readPower();

    updatePowerStats(power, batteryVoltage, dt);

    SYS.battery_voltage = batteryVoltage;

    SYS.battery_pct = powerStats.percentage;

    SYS.current = current;

    SYS.power = power;

    // update estimasi tanggal tiap 1 menit
    if (now - lastPrediction >= 60000UL) {

      lastPrediction = now;

      updateBatteryPredictionDate();
    }

    // simpan energy baterai tiap 10 menit
    if (now - lastSave >= 600000UL) {

      lastSave = now;

      saveEnergyStats();
    }

    // log debug tiap 2 menit
    static unsigned long lastLog = 0;
    if (now - lastLog >= 120000UL) {

      lastLog = now;

      logToFile(
        "🔋 %.0f% % | %.2f V | %.2f mA | %.2f mW | %.0f hari",

        SYS.battery_pct,
        batteryVoltage,
        current,
        power,
        powerStats.estimated_days_left);
    }

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}
