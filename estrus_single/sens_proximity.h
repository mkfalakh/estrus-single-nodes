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

bool isSensor1Dirty();
bool isSensor2Dirty();

void updateDirtyDetection(
  bool s1,
  bool s2);

void resetDirtyDetection();
