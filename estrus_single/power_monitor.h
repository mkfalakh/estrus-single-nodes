#pragma once

typedef struct {
  float energy_mWh;   // total konsumsi
  float avgPower_mW;  // rata-rata power
  float battery_capacity_mWh;
  float remaining_mWh;
  float percentage;
  float estimated_hours_left;

  float estimated_days_left;
  char estimated_date[20];     // 🔥 "YYYY-MM-DD"

} PowerStats;

extern PowerStats powerStats;

void updatePowerStats(float power_mW, float voltage, float dt_seconds);
void updateBatteryPredictionDate();