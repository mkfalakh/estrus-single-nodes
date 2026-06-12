#include "esp32-hal-cpu.h"
#include "wifi_manager.h"
#include "logger.h"
#include "config.h"
#include "config_runtime.h"
#include "system_state.h"
#include <WiFi.h>
#include <Arduino.h>

bool wifiEnabled = true;
unsigned long lastClientTime = 0;

String getAPSSID() {

  return "ESTRUS-" + String(sysConfig.node_id);  // SSID: ESTRUS-NODE-xx
}

void initWiFi() {

  WiFi.mode(WIFI_AP);

  String ssid = getAPSSID();

  bool result = WiFi.softAP(
    ssid.c_str(),
    sysConfig.ap_password);

  if (!result) {
    logToFile("❌ WiFi AP gagal init");
    return;
  }

  IPAddress IP = WiFi.softAPIP();

  logToFile("✅ AP OK");
  logToFile("🛜 Masuk ke WiFi: %s", ssid.c_str());
  logToFile("🌐 Buka di Browser: " + IP.toString());  // 192.168.4.1
}

// WIFI AP ENABLE
void enableWiFiAP() {

  if (wifiEnabled)
    return;

  WiFi.mode(WIFI_AP);

  setCpuFrequencyMhz(240);

  delay(100);

  String ssid = getAPSSID();

  bool result = WiFi.softAP(
    ssid.c_str(),
    sysConfig.ap_password);

  if (!result) {

    logToFile(
      "❌ WiFi AP gagal diaktifkan");

    wifiEnabled = false;

    return;
  }

  wifiEnabled = true;

  lastClientTime = millis();

  logToFile(
    "📡 WiFi AP ENABLED");
}

// WIFI AP DISABLE
void disableWiFiAP() {

  if (!wifiEnabled)
    return;

  WiFi.softAPdisconnect(true);

  delay(100);

  WiFi.mode(WIFI_OFF);

  setCpuFrequencyMhz(80);

  wifiEnabled = false;

  logToFile(
    "📴 WiFi AP DISABLED");
}
