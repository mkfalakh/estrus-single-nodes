#pragma once
#include <Arduino.h>

bool initINA226();
float readBusVoltage();
float readCurrent();
float readPower();

bool checkINAHealth();