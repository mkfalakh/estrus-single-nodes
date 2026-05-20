#pragma once
#include <Arduino.h>
// #include "config.h"

typedef struct {
  // char node_id[16]; // id dari sysConfig.node_id (config_runtime.h)
  char timestamp[32];

  float voltage;
  float current;
  float power;
  float battery_percent;

  int activity_sensor1;
  int activity_sensor2;
  int total_activity;  // jumlah trigger

  float score;
  bool estrus;  // 0 / 1 (hasil analisis)

} SensorData;

extern QueueHandle_t sensorQueue;

void initCSVWriter();

void csvWriterTask(void *pv);

String getCSVPath();  // folder data/nodeid-tahun-bulan-tgl | write data ke .csv
