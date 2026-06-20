#include "config_runtime.h"
#include "nvs_flash.h"
#include "rtc_manager.h"
#include "power_monitor.h"
#include "logger.h"
#include "config.h"
#include "device_identity.h"
#include <Preferences.h>

SystemConfig sysConfig;

bool pendingRestart = false;
unsigned long restartAt = 0;

// Validasi Animal ID
bool isValidAnimalId(const String &id) {

  // panjang aman
  if (id.length() < 3 || id.length() >= ANIMAL_ID_MAX)
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

bool setAnimalId(const String &id) {

  if (!isValidAnimalId(id)) {
    return false;
  }

  memset(sysConfig.animal_id, 0, ANIMAL_ID_MAX);

  strncpy(
    sysConfig.animal_id,
    id.c_str(),
    ANIMAL_ID_MAX - 1);

  return true;
}

// Validasi Node ID
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

// Load Config
void loadConfig() {

  bool useDefault = false;
  bool configChanged = false;

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

      // Load Config | Device
      // NODE ID
      String tmp_node = prefs.getString("node_id", "");
      if (!setNodeId(tmp_node)) {

        // null kan
        memset(sysConfig.node_id, 0, sizeof(sysConfig.node_id));

        logToFile("⚠️ Invalid Node ID → generate default");
      }

      // ANIMAL ID
      String tmp_animal = prefs.getString("animal_id", "");
      if (!setAnimalId(tmp_animal)) {

        setAnimalId("SAPI-00");

        logToFile("⚠️ Invalid Cow ID → reset default");
      }

      // AP Password
      String tmp_pass = prefs.getString("ap_pass", "");

      // null kan
      memset(sysConfig.ap_password, 0, sizeof(sysConfig.ap_password));

      strncpy(sysConfig.ap_password, tmp_pass.c_str(),
              sizeof(sysConfig.ap_password) - 1);


      // generate default Node ID
      if (strlen(sysConfig.node_id) == 0) {

        String id = generateDefaultNodeId();

        strncpy(sysConfig.node_id, id.c_str(),
                sizeof(sysConfig.node_id) - 1);

        configChanged = true;

        logToFile(
          "🆔 Generated Node ID: %s",
          sysConfig.node_id);
      }

      // generate default WIFI AP Password
      if (strlen(sysConfig.ap_password) < 8) {

        String pass = generateDefaultAPPassword();

        strncpy(sysConfig.ap_password, pass.c_str(),
                sizeof(sysConfig.ap_password) - 1);

        configChanged = true;

        logToFile(
          "🔑 Default AP password generated");
      }

      sysConfig.prox_active_low = prefs.getBool("prox_low", true);
      sysConfig.alarm_enabled = prefs.getBool("alarm", true);

      // Model Estrus
      sysConfig.record_interval_sec = prefs.getUShort("record", 10);
      sysConfig.retention_days = prefs.getUChar("retain", 7);
      sysConfig.partition_hours = prefs.getUChar("part", 1);
      sysConfig.estrus_threshold_pct = prefs.getFloat("estrus", 6.0f);
      sysConfig.stop_after_alarm = prefs.getBool("stop_alarm", true);
      sysConfig.min_baseline_samples = prefs.getUShort("base_sample", 10);
      sysConfig.dirty_timeout_hours = prefs.getUChar("dirty_hour", 2);

      // Battery
      sysConfig.current_threshold = prefs.getFloat("curr_th", 150.0);
      sysConfig.power_threshold = prefs.getFloat("pow_th", 600.0);
      powerStats.energy_mWh = prefs.getFloat("energy", 0);

      prefs.end();

      if (configChanged) {
        saveConfig();
      }

      Serial.println("📥 Config Loaded!");
    }
  }

  // 🔥 fallback default
  if (useDefault) {

    // Device

    memset(&sysConfig, 0, sizeof(sysConfig));

    String id = generateDefaultNodeId();
    strncpy(sysConfig.node_id, id.c_str(), sizeof(sysConfig.node_id) - 1);

    String pass = generateDefaultAPPassword();
    strncpy(sysConfig.ap_password, pass.c_str(), sizeof(sysConfig.ap_password) - 1);

    strncpy(sysConfig.animal_id, "SAPI-00", sizeof(sysConfig.animal_id) - 1);
    sysConfig.prox_active_low = true;  // true = LOW trigger | false = HIGH trigger
    sysConfig.alarm_enabled = true;    // ingin alarm aktif/mati

    // Model Estrus
    sysConfig.record_interval_sec = 10;     // 10 - 3600
    sysConfig.retention_days = 7;           // 1 - 14
    sysConfig.partition_hours = 1;          // 1 - 24
    sysConfig.estrus_threshold_pct = 6.0f;  // 0.1 - 100 %
    sysConfig.stop_after_alarm = true;
    sysConfig.min_baseline_samples = 10;   // 10 - 1000 | untuk validasi baseline
    sysConfig.dirty_timeout_hours = 2;  // 1 - 24 | batas waktu untuk mengetahui sensor kotor atau tidak

    // Battery
    sysConfig.current_threshold = 150.0;  // 100 - 150 mA | alert batas maksimal arus batre
    sysConfig.power_threshold = 600.0;    // 400 - 600 mW | alert batas maksimal power batre
    powerStats.energy_mWh = 0;

    saveConfig();

    // Debug
    Serial.println("⚙️ Default Config Loaded!");

    // Serial.println(String("node id: ") + sysConfig.node_id);
    // Serial.println(String("animal id: ") + sysConfig.animal_id);
    // Serial.println(String("AP password: ") + sysConfig.ap_password);
    // Serial.println(String("prox mode: ") + sysConfig.prox_active_low);
    // Serial.println(String("alarm enable ? ") + sysConfig.alarm_enabled);

    // Serial.println(String("rec interval sec: ") + sysConfig.record_interval_sec);
    // Serial.println(String("retention days: ") + sysConfig.retention_days);
    // Serial.println(String("partition hours: ") + sysConfig.partition_hours);
    // Serial.println(String("estrus threshold: ") + sysConfig.estrus_threshold_pct);
    // Serial.println(String("stop after alarm ? ") + sysConfig.stop_after_alarm);
    // Serial.println(String("baseline samples: ") + sysConfig.min_baseline_samples);
    // Serial.println(String("dirty samples: ") + sysConfig.dirty_timeout_hours);

    // Serial.println(String("current threshold: ") + sysConfig.current_threshold);
    // Serial.println(String("power threshold: ") + sysConfig.power_threshold);
    // Serial.println(String("energy: ") + powerStats.energy_mWh);
  }
}

void saveConfig() {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    logToFile("❌ Gagal menyimpan config!");
    return;
  }

  // Device
  prefs.putString("node_id", String(sysConfig.node_id));
  prefs.putString("animal_id", String(sysConfig.animal_id));
  prefs.putString("ap_pass", String(sysConfig.ap_password));
  prefs.putBool("prox_low", sysConfig.prox_active_low);
  prefs.putBool("alarm", sysConfig.alarm_enabled);

  // Model Estrus
  prefs.putUShort("record", sysConfig.record_interval_sec);
  prefs.putUChar("retain", sysConfig.retention_days);
  prefs.putUChar("part", sysConfig.partition_hours);
  prefs.putFloat("estrus", sysConfig.estrus_threshold_pct);
  prefs.putBool("stop_alarm", sysConfig.stop_after_alarm);
  prefs.putUShort("base_sample", sysConfig.min_baseline_samples);
  prefs.putUChar("dirty_hour", sysConfig.dirty_timeout_hours);

  // Battery
  prefs.putFloat("curr_th", sysConfig.current_threshold);
  prefs.putFloat("pow_th", sysConfig.power_threshold);
  prefs.putFloat("energy", powerStats.energy_mWh);

  prefs.end();

  logToFile("💾 Config Saved");
}

void resetConfig() {

  Preferences prefs;

  if (!prefs.begin("sapi", false)) {
    return;
  }

  prefs.clear();

  prefs.end();

  memset(&sysConfig, 0, sizeof(sysConfig));

  loadConfig();

  saveConfig();

  logToFile("♻️ Config reset factory");

  ESP.restart();
}
