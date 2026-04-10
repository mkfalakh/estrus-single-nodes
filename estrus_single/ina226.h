#pragma once
#include <Arduino.h>

void initINA226();
float readVoltage();
float readCurrent();
float readPower();