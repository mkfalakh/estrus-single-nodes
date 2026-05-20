/* Wiring:
Hitam (data) = Proximity-1: 7 | Proximity-2: 15
Biru (gnd) = GND
Coklat (vcc) = 5v
*/

#pragma once
#include <Arduino.h>

void sensorTask(void *pv);

void setProximityActiveLow(bool activeLow);
bool readProx1();
bool readProx2();

void initProximity();
bool detectEstrusAdvanced(int a1, int a2, int hour);
float getLastEstrusScore();
