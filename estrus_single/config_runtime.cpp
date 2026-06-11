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
      String tmp_node = prefs.getString("node_id", "NODE-01");
      if (!setNodeId(tmp_node)) {
        setNodeId("NODE-01");

        logToFile("⚠️ Invalid Node ID → reset default");
      }

      // ANIMAL ID
      String tmp_animal = prefs.getString("animal_id", "COW-01");
      if (!setAnimalId(tmp_animal)) {
        setAnimalId("COW-01");

        logToFile("⚠️ Invalid Cow ID → reset default");
      }

      sysConfig.prox_active_low = prefs.getBool("prox_low", true);
      sysConfig.alarm_enabled = prefs.getBool("alarm", true);
      // prefs.getString("animal_id", "COW-01").toCharArray(sysConfig.animal_id, sizeof(sysConfig.animal_id));
      // sysConfig.interval_hours = prefs.getInt("interval", 1);

      // Model Estrus
      sysConfig.record_interval_sec = prefs.getUShort("record", 30);
      sysConfig.retention_days = prefs.getUChar("retain", 14);
      sysConfig.partition_hours = prefs.getUChar("part", 3);
      sysConfig.estrus_threshold_pct = prefs.getFloat("estrus", 6.0f);
      sysConfig.stop_after_alarm = prefs.getBool("stop_alarm", true);
      sysConfig.min_baseline_samples = prefs.getUChar("base_sample", 300);
      sysConfig.dirty_timeout_samples = prefs.getUChar("dirty_sample", 240);

      // Battery
      sysConfig.current_threshold = prefs.getFloat("curr_th", 150.0);
      sysConfig.power_threshold = prefs.getFloat("pow_th", 600.0);
      powerStats.energy_mWh = prefs.getFloat("energy", 0);

      prefs.end();

      Serial.println("📥 Config Loaded!");
    }
  }

  // 🔥 fallback default
  if (useDefault) {

    // Device
    memset(&sysConfig, 0, sizeof(sysConfig));
    strncpy(sysConfig.node_id, "NODE-01", sizeof(sysConfig.node_id) - 1);  // ubah sesuai device id
    strncpy(sysConfig.animal_id, "COW-01", sizeof(sysConfig.animal_id) - 1);
    sysConfig.prox_active_low = true;  // true = LOW trigger | false = HIGH trigger
    sysConfig.alarm_enabled = true;    // ingin alarm aktif/mati
    // sysConfig.interval_hours = 1;      // interval dalam jam

    // Model Estrus
    sysConfig.record_interval_sec = 30;     // 10 - 3600
    sysConfig.retention_days = 14;          // 1 - 14
    sysConfig.partition_hours = 3;          // must divide 24
    sysConfig.estrus_threshold_pct = 6.0f;  // 0.1 - 100 %
    sysConfig.stop_after_alarm = true;
    sysConfig.min_baseline_samples = 300;   // 360 sample untuk interval = 30s & partition = 3h | untuk validasi baseline
    sysConfig.dirty_timeout_samples = 240;  // sample untuk mengetahui sensor kotor atau tidak

    // Battery
    sysConfig.current_threshold = 150.0;  // alert batas maksimal arus batre | 100 - 150 mA
    sysConfig.power_threshold = 600.0;    // alert batas maksimal power batre | 400 - 600 mW
    powerStats.energy_mWh = 0;

    // Debug
    Serial.println("⚙️ Default Config Loaded!");

    // Serial.println(sysConfig.interval_hours);
    Serial.println(sysConfig.node_id);
    Serial.println(sysConfig.animal_id);
    Serial.println(sysConfig.prox_active_low);
    Serial.println(sysConfig.alarm_enabled);

    Serial.println(sysConfig.record_interval_sec);
    Serial.println(sysConfig.retention_days);
    Serial.println(sysConfig.partition_hours);
    Serial.println(sysConfig.estrus_threshold_pct);
    Serial.println(sysConfig.stop_after_alarm);
    Serial.println(sysConfig.min_baseline_samples);
    Serial.println(sysConfig.dirty_timeout_samples);

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

  // Device
  // prefs.putInt("interval", sysConfig.interval_hours);
  prefs.putString("node_id", String(sysConfig.node_id));
  prefs.putString("animal_id", String(sysConfig.animal_id));
  prefs.putBool("prox_low", sysConfig.prox_active_low);
  prefs.putBool("alarm", sysConfig.alarm_enabled);

  // Model Estrus
  prefs.putUShort("record", sysConfig.record_interval_sec);
  prefs.putUChar("retain", sysConfig.retention_days);
  prefs.putUChar("part", sysConfig.partition_hours);
  prefs.putFloat("estrus", sysConfig.estrus_threshold_pct);
  prefs.putBool("stop_alarm", sysConfig.stop_after_alarm);
  prefs.putUChar("base_sample", sysConfig.min_baseline_samples);
  prefs.putUChar("dirty_sample", sysConfig.dirty_timeout_samples);

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
    logToFile("❌ Gagal reset config!");
    return;
  }
  prefs.clear();
  prefs.end();

  logToFile("♻️ Config reset");
  loadConfig();
}
