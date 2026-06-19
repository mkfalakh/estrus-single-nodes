#pragma once
#include <Arduino.h>

bool initINA226();
float readVoltage();
float readCurrent();
float readPower();

bool checkINAHealth();