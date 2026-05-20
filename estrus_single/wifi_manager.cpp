#include "esp32-hal-cpu.h"
#include "wifi_manager.h"
#include "logger.h"
#include "config.h"
#include "config_runtime.h"
#include "system_state.h"
#include <WiFi.h>
#include <Arduino.h>

const char* WIFI_PASSWORD = "estrus2026";

void initWiFi() {

  WiFi.mode(WIFI_AP);

  String ssid = "ESTRUS-";  // cth: ESTRUS-NODE-01 | SSID
  ssid += sysConfig.node_id;

  bool result = WiFi.softAP(
    ssid.c_str(),
    WIFI_PASSWORD);

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

  String ssid = "ESTRUS-";  // cth: ESTRUS-NODE-01 | SSID
  ssid += sysConfig.node_id;

  bool ok = WiFi.softAP(
    ssid.c_str(),
    WIFI_PASSWORD);

  if (!ok) {

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
