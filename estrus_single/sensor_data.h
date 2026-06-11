#pragma once

struct SensorData {
  char timestamp[24];

  bool sensor1_state;
  bool sensor2_state;
  bool sensor1_dirty;
  bool sensor2_dirty;

  float deviation_pct;
  bool estrus;

  float voltage;
  float current;
  float battery_pct;
};
