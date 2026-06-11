#pragma once
#include <Arduino.h>

extern QueueHandle_t sensorQueue;

void initCSVWriter();

void csvWriterTask(void *pv);

String getCSVPath();  // folder data/nodeid-tahun-bulan-tgl | write data ke .csv
