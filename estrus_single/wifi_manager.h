#pragma once
#include <Arduino.h>

void initWiFi();
// void enableWiFiAP();
// void disableWiFiAP();
bool isWiFiEnabled();

extern bool wifiEnabled;
extern unsigned long lastClientTime;
String getAPSSID();
