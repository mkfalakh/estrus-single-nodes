#include "power_monitor.h"
#include "rtc_manager.h"

PowerStats powerStats = {
  0,      // energy mWh | total konsumsi batre | default: 0
  0,      // avg power mW | rata-rata power | default: 0
  22200,  // kapasitas batre mWh | value 22200 = 3.7V * 6000mAh | rumus: value = tegangan minimum batre * kapasitas batre
  22200, // remaining mWh | default: samakan dengan value kapasitas batre mWh
  0, // persentase batre | default: 0
  0 // estimasi habis batre dalam beberapa jam | default: 0
};

void updatePowerStats(float power_mW, float voltage, float dt) {

  // 🔥 integrasi energi
  float energy = power_mW * (dt / 3600.0);  // mWh
  powerStats.energy_mWh += energy;

  // 🔥 moving average sederhana
  powerStats.avgPower_mW =
    (powerStats.avgPower_mW * 0.9f) + (power_mW * 0.1f);

  // 🔋 sisa energi
  powerStats.remaining_mWh =
    powerStats.battery_capacity_mWh - powerStats.energy_mWh;

  if (powerStats.remaining_mWh < 0)
    powerStats.remaining_mWh = 0;

  // 🔋 %
  powerStats.percentage =
    (powerStats.remaining_mWh / powerStats.battery_capacity_mWh) * 100.0;

  // estimasi waktu
  if (powerStats.avgPower_mW > 0) {
    powerStats.estimated_hours_left =
      powerStats.remaining_mWh / powerStats.avgPower_mW;
  } else {
    powerStats.estimated_hours_left = 0;
  }

  if (powerStats.avgPower_mW < 1) {
    powerStats.estimated_hours_left = 0;
    powerStats.estimated_days_left = 0;
  }

  // 🔥 hitung hari
  powerStats.estimated_days_left =
    powerStats.estimated_hours_left / 24.0f;
}

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
