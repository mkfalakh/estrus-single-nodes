#pragma once
#include <Arduino.h>

extern QueueHandle_t sensorQueue;

void initCSVWriter();
void csvWriterTask(void *pv);
