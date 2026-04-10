#pragma once
#include <Arduino.h>

typedef struct {
  char node_id[16];
  char timestamp[32];
  float voltage;
  float current;
} SensorData;

void initSDCard();
void logToCSV(const SensorData &data);
String getFilenameFromDate(const char *timestamp);