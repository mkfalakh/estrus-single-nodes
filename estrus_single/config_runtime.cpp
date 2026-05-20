#include "config_runtime.h"
#include "nvs_flash.h"
#include "rtc_manager.h"
#include "power_monitor.h"
#include "logger.h"
#include "config.h"
#include <Preferences.h>

SystemConfig sysConfig;

bool pendingRestart = false;
unsigned long restartAt = 0;

// String getNodeId() {
//   return String(sysConfig.node_id);
// }

bool isValidNodeId(const String &id) {

  // panjang aman
  if (id.length() < 3 || id.length() >= NODE_ID_MAX)
    return false;

  // hanya huruf angka dash underscore
  for (size_t i = 0; i < id.length(); i++) {

    char c = id[i];

    bool ok =
      isAlphaNumeric(c) || c == '-' || c == '_';

    if (!ok)
      return false;
  }

  return true;
}

bool setNodeId(const String &id) {

  if (!isValidNodeId(id)) {
    return false;
  }

  memset(sysConfig.node_id, 0, NODE_ID_MAX);

  strncpy(
    sysConfig.node_id,
    id.c_str(),
    NODE_ID_MAX - 1);

  return true;
}

void loadConfig() {

  bool useDefault = false;

  // 🔥 init NVS
  esp_err_t err = nvs_flash_init();

  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

    Serial.println("⚠️ NVS bermasalah, reset...");
    nvs_flash_erase();
    err = nvs_flash_init();
  }

  if (err != ESP_OK) {
    Serial.println("❌ NVS init gagal");
    useDefault = true;
  }

  if (!useDefault) {
    Preferences prefs;

    if (!prefs.begin("sapi", true)) {
      Serial.println("⚠️ Tidak ada config yang tersimpan!");
      useDefault = true;
    } else {

      // 🔥 load config
      String tmp = prefs.getString("node_id", "NODE-01");
      if (!setNodeId(tmp)) {
        setNodeId("NODE-01");

        logToFile("⚠️ Invalid Node ID → reset default");
      }

      sysConfig.prox_active_low = prefs.getBool("prox_low", true);
      sysConfig.interval_hours = prefs.getInt("interval", 1);
      sysConfig.buzzer_enabled = prefs.getBool("buzzer", true);

      sysConfig.score_threshold = prefs.getFloat("score_th", 0.65);
      sysConfig.ratio_trigger = prefs.getFloat("ratio_tr", 1.5);
      sysConfig.persist_required = prefs.getInt("persist", 3);
      sysConfig.ema_alpha = prefs.getFloat("ema", 0.1);

      sysConfig.activity_min = prefs.getInt("act_min", 5);
      sysConfig.balance_min = prefs.getFloat("bal_min", 0.3);

      sysConfig.current_threshold = prefs.getFloat("curr_th", 500.0);
      sysConfig.power_threshold = prefs.getFloat("pow_th", 2000.0);
      powerStats.energy_mWh = prefs.getFloat("energy", 0);

      prefs.end();

      Serial.println("📥 Config Loaded!");
    }
  }

  // 🔥 fallback default
  if (useDefault) {

    memset(&sysConfig, 0, sizeof(sysConfig));
    strncpy(sysConfig.node_id, "NODE-01", sizeof(sysConfig.node_id) - 1);  // ubah sesuai device id
    
    sysConfig.prox_active_low = true;  // true = LOW trigger | false = HIGH trigger
    sysConfig.interval_hours = 1;      // interval dalam jam
    sysConfig.buzzer_enabled = true;   // alarm aktif setelah interval

    sysConfig.score_threshold = 0.65;  // sensitivitas
    sysConfig.ratio_trigger = 1.5;     // ratio trigger
    sysConfig.persist_required = 3;    // noise filter
    sysConfig.ema_alpha = 0.1;         // adaptasi

    sysConfig.activity_min = 5;
    sysConfig.balance_min = 0.3;

    sysConfig.current_threshold = 500.0;  // alert batas maksimal arus batre
    sysConfig.power_threshold = 2000.0;   // alert batas maksimal power batre
    powerStats.energy_mWh = 0;

    Serial.println("⚙️ Default Config Loaded!");
    
    Serial.println(sysConfig.node_id);
    Serial.println(sysConfig.prox_active_low);
    Serial.println(sysConfig.interval_hours);
    Serial.println(sysConfig.buzzer_enabled);
    Serial.println(sysConfig.score_threshold);
    Serial.println(sysConfig.ratio_trigger);
    Serial.println(sysConfig.persist_required);
    Serial.println(sysConfig.ema_alpha);
    Serial.println(sysConfig.activity_min);
    Serial.println(sysConfig.balance_min);
    Serial.println(sysConfig.current_threshold);
    Serial.println(sysConfig.power_threshold);
    Serial.println(powerStats.energy_mWh);

  }
}

void saveConfig() {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    logToFile("❌ Gagal menyimpan config!");
    return;
  }

  prefs.putString("node_id", String(sysConfig.node_id));
  prefs.putBool("prox_low", sysConfig.prox_active_low);
  prefs.putInt("interval", sysConfig.interval_hours);
  prefs.putBool("buzzer", sysConfig.buzzer_enabled);

  prefs.putFloat("score_th", sysConfig.score_threshold);
  prefs.putFloat("ratio_tr", sysConfig.ratio_trigger);
  prefs.putInt("persist", sysConfig.persist_required);
  prefs.putFloat("ema", sysConfig.ema_alpha);

  prefs.putInt("act_min", sysConfig.activity_min);
  prefs.putFloat("bal_min", sysConfig.balance_min);

  prefs.putFloat("curr_th", sysConfig.current_threshold);
  prefs.putFloat("pow_th", sysConfig.power_threshold);
  prefs.putFloat("energy", powerStats.energy_mWh);

  prefs.end();

  logToFile("💾 Config Saved");
}

void resetConfig() {
  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    logToFile("❌ Gagal reset config!");
    return;
  }
  prefs.clear();
  prefs.end();

  logToFile("♻️ Config reset");
  loadConfig();
}
