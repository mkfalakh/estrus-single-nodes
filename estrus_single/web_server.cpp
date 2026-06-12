#include "web_server.h"
#include "auth.h"
#include "crypto.h"
#include "config.h"
#include "rtc_manager.h"
#include "csv_reverse.h"
#include "buzzer.h"
#include "config_runtime.h"
#include "power_monitor.h"
#include "system_state.h"
#include "logger.h"
#include "sens_proximity.h"
#include "storage_stats.h"
#include "wifi_manager.h"
#include "csv_writer.h"
#include <WebServer.h>
#include <SD.h>
#include <Arduino.h>

WebServer server(80);

String jsonEscape(const String& s) {

  String out;

  for (char c : s) {

    if (c == '\"')
      out += '\\';

    out += c;
  }

  return out;
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  // if (filename.endsWith(".png")) return "image/png";
  // if (filename.endsWith(".jpg")) return "image/jpeg";
  return "text/plain";
}

String escapeJson(const char* s) {

  String out;

  while (*s) {

    switch (*s) {

      case '\"':
        out += "\\\"";
        break;

      case '\\':
        out += "\\\\";
        break;

      case '\n':
        out += "\\n";
        break;

      case '\r':
        out += "\\r";
        break;

      case '\t':
        out += "\\t";
        break;

      default:
        out += *s;
        break;
    }

    s++;
  }

  return out;
}

static String buildJSONRow(const String& line) {
  String s = "\"";
  s += line;
  s += "\"";
  return s;
}

bool handleFileRead(String path) {

  if (path.endsWith("/")) path += "index.html";

  String fullPath = "/www" + path;

  if (!SD.exists(fullPath)) {
    logToFile("❌ File tidak ada: " + fullPath);
    return false;
  }

  File file = SD.open(fullPath);
  if (!file) return false;

  String contentType = getContentType(fullPath);

  server.streamFile(file, contentType);
  file.close();

  return true;
}

bool isAuthenticated() {

  if (!server.hasHeader("Cookie")) {
    logToFile("❌ No Cookie Header");
    return false;
  }

  String cookie = server.header("Cookie");
  logToFile("🍪 Cookie: " + cookie);  // 🔥 DEBUG

  String sessionId = getSessionFromHeader(cookie);
  logToFile("🔑 SessionID: " + sessionId);  // 🔥 DEBUG

  if (sessionId == "") return false;

  lastClientTime = millis();

  return validateSession(sessionId);
}

void handleCheckAuth() {

  // if (!isAuthenticated()) {

  //   server.send(
  //     401,
  //     "application/json",
  //     "{\"auth\":false}");

  //   return;
  // }

  String json = "{";

  json += "\"auth\":true,";
  json += "\"node_id\":\"";
  json += String(sysConfig.node_id);
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}

void handleLogout() {
  logToFile("✅ Logout sukses");
  server.sendHeader("Set-Cookie", "ESPSESSIONID=deleted; Path=/; Max-Age=0");
  server.send(200, "application/json", "{\"success\":true}");
}

void handleLogin() {

  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "application/json", "{\"success\":false}");
    return;
  }

  String user = server.arg("user");
  String pass = server.arg("pass");

  // ❌ user salah
  if (user != USER) {
    logToFile("❌ Login gagal (user salah): " + user);
    // vTaskDelay(pdMS_TO_TICKS(300));
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }

  String hashedInput = hashPassword(pass, SALT);

  // ❌ password salah
  if (hashedInput != HASHED_PASS) {
    logToFile("❌ Login gagal (password salah)");
    // vTaskDelay(pdMS_TO_TICKS(300));
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }

  // ✅ login berhasil
  String sessionId;
  if (!createSession(sessionId)) {
    server.send(500, "application/json", "{\"error\":\"session penuh\"}");
    return;
  }

  logToFile("✅ Login sukses: " + user);

  server.sendHeader("Set-Cookie", "ESPSESSIONID=" + sessionId + "; Path=/; SameSite=Lax; Max-Age=3600");

  server.send(200, "application/json", "{\"success\":true}");
}

void handleSystemStatus() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  json += "\"error\":" + String(sysIsError() ? 1 : 0) + ",";
  json += "\"sensor_dirty\":" + String(sysIsSensorDirty() ? 1 : 0) + ",";
  json += "\"low_battery\":" + String(sysIsLowBattery() ? 1 : 0) + ",";
  json += "\"alarm\":" + String(sysIsAlarm() ? 1 : 0) + ",";
  json += "\"wifi\":" + String(wifiEnabled ? 1 : 0) + ",";
  json += "\"sd_mutex\":" + String(sdMutex != NULL);

  json += "}";

  server.send(200, "application/json", json);
}

void handleHistory() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String date = server.arg("date");

  if (date.length() != 10 || date[4] != '-' || date[7] != '-') {

    server.send(
      400,
      "application/json",
      "{\"error\":\"invalid date\"}");

    return;
  }

  int page = 0;
  int limit = 20;

  String filename = "/data/" + String(sysConfig.node_id) + "-" + date + ".csv";

  if (!SD.exists(filename)) {

    server.send(
      200,
      "application/json",
      "{\"rows\":[],\"has_next\":false}");

    return;
  }

  File file = SD.open(filename);

  if (!file) {

    sysSetSD(false);

    server.send(
      500,
      "application/json",
      "{\"error\":\"sd\"}");

    return;
  }

  sysSetSD(true);

  // PAGE
  if (server.hasArg("page")) {

    page = (int)server.arg("page").toInt();

    if (page < 0) {
      page = 0;
    }
  }

  // LIMIT
  if (server.hasArg("limit")) {

    long l = server.arg("limit").toInt();

    l = constrain(l, 1L, 100L);

    limit = (int)l;

    // limit = constrain(
    //   server.arg("limit").toInt(),
    //   1,
    //   100);
  }

  // char lines[LIMIT][128]; // alternatif jika PSRAM tidak bisa

  // menggunakan PSRAM
  char(*lines)[128] =
    (char(*)[128])ps_malloc(
      limit * sizeof(*lines));

  if (!lines) {

    server.send(
      500,
      "application/json",
      "{\"error\":\"psram\"}");

    file.close();

    return;
  }

  // int count = readLastLines(file, lines, LIMIT);

  bool hasNext = false;

  int count =
    readCsvPage(
      file,
      lines,
      page,
      limit,
      hasNext);

  file.close();


  String json = "{";

  json += "\"page\":";
  json += String(page);

  json += ",\"limit\":";
  json += String(limit);

  json += ",\"count\":";
  json += String(count);

  json += ",\"rows\":[";

  for (int i = 0; i < count; i++) {
    json += "\"";
    json += escapeJson(lines[i]);
    json += "\"";

    if (i < count - 1) json += ",";
  }

  json += "],";

  json += "\"has_next\":";
  json += String(
    hasNext ? "true" : "false");

  json += "}";

  free(lines);

  server.send(200, "application/json", json);
}

// ===== HANDLE BUZZER =====
void handleStatusBuzzer() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  json += "\"buzzer_active\":" + String(SYS.buzzer_active ? "true" : "false") + ",";
  json += "\"alarm_enabled\":" + String(sysConfig.alarm_enabled ? "true" : "false") + ",";
  json += "\"stop_after_alarm\":" + String(sysConfig.stop_after_alarm ? "true" : "false") + ",";
  json += "\"alarm_ack\":" + String(isAlarmAcknowledged() ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json);
}

void handleStopBuzzer() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  if (sysConfig.stop_after_alarm) {

    acknowledgeAlarm();

    logToFile(
      "🔕 Alarm acknowledged");

  } else {

    sysStopAlarm();

    logToFile(
      "🔕 Alarm stopped");
  }

  server.send(200, "application/json", "{\"success\":true}");
}

// ===== HANDLE DOWNLOAD BUTTON =====
void handleDownload() {

  // if (!isAuthenticated()) {
  //   server.send(401, "text/plain", "Unauthorized");
  //   return;
  // }

  if (!server.hasArg("date")) {
    server.send(400, "text/plain", "Missing date");
    return;
  }

  String date = server.arg("date");
  String filename = "/data/" + String(sysConfig.node_id) + "-" + date + ".csv";

  File file = SD.open(filename);
  if (!file) {
    sysSetSD(false);

    server.send(404, "text/plain", "File not found");
    return;
  }

  sysSetSD(true);

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=" + String(sysConfig.node_id) + "-" + date + ".csv");

  server.streamFile(file, "text/csv");
  file.close();
}

// ===== GET CONFIG DARI MEMORY ESP =====
void handleGetConfig() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  // DEVICE
  json += "\"node_id\":\"";
  json += String(sysConfig.node_id);
  json += "\",";

  json += "\"animal_id\":\"";
  json += String(sysConfig.animal_id);
  json += "\",";

  json += "\"ap_password\":\"";
  json += String(sysConfig.ap_password);
  json += "\",";

  json += "\"prox_low\":";
  json += String(sysConfig.prox_active_low ? 1 : 0);
  json += ",";

  json += "\"alarm_enabled\":";
  json += String(sysConfig.alarm_enabled ? 1 : 0);
  json += ",";

  // MODEL ESTRUS
  json += "\"record_interval_sec\":";
  json += String(sysConfig.record_interval_sec);
  json += ",";

  json += "\"retention_days\":";
  json += String(sysConfig.retention_days);
  json += ",";

  json += "\"partition_hours\":";
  json += String(sysConfig.partition_hours);
  json += ",";

  json += "\"estrus_threshold_pct\":";
  json += String(sysConfig.estrus_threshold_pct, 2);
  json += ",";

  json += "\"stop_after_alarm\":";
  json += String(sysConfig.stop_after_alarm ? 1 : 0);
  json += ",";

  json += "\"min_baseline_samples\":";
  json += String(sysConfig.min_baseline_samples);
  json += ",";

  json += "\"dirty_timeout_samples\":";
  json += String(sysConfig.dirty_timeout_samples);
  json += ",";

  // BATTERY
  json += "\"current_threshold\":" + String(sysConfig.current_threshold) + ",";
  json += "\"power_threshold\":" + String(sysConfig.power_threshold);

  json += "}";

  server.send(200, "application/json", json);
}

// ===== SET/SAVE CONFIG DARI USER =====
void handleSetConfig() {

  // ========================
  // AUTH
  // ========================
  // if (!isAuthenticated()) {

  //   server.send(
  //     401,
  //     "application/json",
  //     "{\"error\":\"unauthorized\"}");

  //   return;
  // }

  // ========================
  // TEMP CONFIG
  // ========================
  SystemConfig temp = sysConfig;

  bool needRestart = false;

  // ========================
  // NODE ID
  // ========================
  if (server.hasArg("node_id")) {

    String id = server.arg("node_id");

    id.trim();

    // restart hanya jika berbeda = true
    if (id != String(sysConfig.node_id)) {

      if (!isValidNodeId(id)) {

        server.send(
          400,
          "application/json",
          "{\"error\":\"invalid node_id cfg\"}");

        return;
      }

      memset(
        temp.node_id,
        0,
        sizeof(temp.node_id));

      strncpy(
        temp.node_id,
        id.c_str(),
        sizeof(temp.node_id) - 1);

      needRestart = true;  // perlu restart device

      logToFile(
        "🔧 Node ID changed: %s",
        temp.node_id);
    }
  }

  // ========================
  // PROX MODE
  // ========================
  if (server.hasArg("prox_low")) {

    String v = server.arg("prox_low");

    if (v != "0" && v != "1") {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid prox_low cfg\"}");

      return;
    }

    temp.prox_active_low = (v == "1");
  }

  // ========================
  // BUZZER/ALARM
  // ========================
  if (server.hasArg("alarm_enabled")) {

    String v = server.arg("alarm_enabled");

    if (v != "0" && v != "1") {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid alarm cfg\"}");

      return;
    }

    temp.alarm_enabled = (v == "1");
  }

  // ========================
  // ANIMAL ID
  // ========================
  if (server.hasArg("animal_id")) {

    String id = server.arg("animal_id");

    id.trim();

    // restart hanya jika berbeda = false
    if (id != String(sysConfig.animal_id)) {

      if (!isValidAnimalId(id)) {

        server.send(
          400,
          "application/json",
          "{\"error\":\"invalid animal_id cfg\"}");

        return;
      }

      memset(
        temp.animal_id,
        0,
        sizeof(temp.animal_id));

      strncpy(
        temp.animal_id,
        id.c_str(),
        sizeof(temp.animal_id) - 1);

      // needRestart = false;  // tidak perlu restart device

      logToFile(
        "🐄 Animal ID changed: %s",
        temp.animal_id);
    }
  }

  // ========================
  // WIFI AP PASSWORD
  // ========================
  if (server.hasArg("ap_password")) {

    String pass =
      server.arg("ap_password");

    pass.trim();

    if (
      pass.length() < 8 || pass.length() > 31) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid ap_password\"}");

      return;
    }

    if (pass != String(sysConfig.ap_password)) {

      memset(
        temp.ap_password,
        0,
        sizeof(temp.ap_password));

      strncpy(
        temp.ap_password,
        pass.c_str(),
        sizeof(temp.ap_password) - 1);

      needRestart = true;

      logToFile(
        "🔑 AP password changed");
    }
  }

  // ========================
  // RECORD INTERVAL SEC
  // ========================
  if (server.hasArg("record_interval_sec")) {

    int v = server.arg("record_interval_sec").toInt();

    if (v < 10 || v > 3600) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid record_interval_sec cfg\"}");

      return;
    }

    temp.record_interval_sec = v;
  }

  // ========================
  // RETENTION DAYS
  // ========================
  if (server.hasArg("retention_days")) {

    int v = server.arg("retention_days").toInt();

    if (v < 1 || v > 14) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid retention_days cfg\"}");

      return;
    }

    temp.retention_days = v;
  }

  // ========================
  // PARTITION HOURS
  // ========================
  if (server.hasArg("partition_hours")) {

    int v = server.arg("partition_hours").toInt();

    if (v < 1 || v > 24 || (24 % v) != 0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid partition_hours cfg\"}");

      return;
    }

    temp.partition_hours = v;
  }

  // ========================
  // ESTRUS THRESHOLD PCT
  // ========================
  if (server.hasArg("estrus_threshold_pct")) {

    float v = server.arg("estrus_threshold_pct").toFloat();

    if (v < 0.0 || v > 100.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid estrus_threshold_pct cfg\"}");

      return;
    }

    temp.estrus_threshold_pct = v;
  }

  // ========================
  // STOP AFTER ALARM
  // ========================
  if (server.hasArg("stop_after_alarm")) {

    String v = server.arg("stop_after_alarm");

    if (v != "0" && v != "1") {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid stop_after_alarm cfg\"}");

      return;
    }

    temp.stop_after_alarm = (v == "1");
  }

  // ========================
  // MIN BASELINE SAMPLES
  // ========================
  if (server.hasArg("min_baseline_samples")) {

    int v = server.arg("min_baseline_samples").toInt();

    if (v < 10 || v > 1000) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid min_baseline_samples cfg\"}");

      return;
    }

    temp.min_baseline_samples = v;
  }

  // ========================
  // DIRTY TIMEOUT SAMPLES
  // ========================
  if (server.hasArg("dirty_timeout_samples")) {

    int v = server.arg("dirty_timeout_samples").toInt();

    if (v < 10 || v > 1000) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid dirty_timeout_samples cfg\"}");

      return;
    }

    temp.dirty_timeout_samples = v;
  }

  // ========================
  // POWER & CURRENT BATTERY
  // ========================
  if (server.hasArg("current_threshold")) {

    float v = server.arg("current_threshold").toFloat();

    if (v < 100.0 || v > 150.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid current_threshold cfg\"}");

      return;
    }

    temp.current_threshold = v;
  }

  // POWER
  if (server.hasArg("power_threshold")) {

    float v = server.arg("power_threshold").toFloat();

    if (v < 400.0 || v > 600.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid power_threshold cfg\"}");

      return;
    }

    temp.power_threshold = v;
  }

  // ========================
  // ALL VALID → APPLY
  // ========================

  // simpan nilai lama sebelum overwrite
  bool proxChanged = temp.prox_active_low != sysConfig.prox_active_low;
  bool alarmChanged = temp.alarm_enabled != sysConfig.alarm_enabled;
  bool intervalChanged = temp.record_interval_sec != sysConfig.record_interval_sec;
  bool partitionChanged = temp.partition_hours != sysConfig.partition_hours;
  bool estrusThresholdChanged = temp.estrus_threshold_pct != sysConfig.estrus_threshold_pct;
  bool retentionChanged = temp.retention_days != sysConfig.retention_days;
  bool stopAlarmChanged = temp.stop_after_alarm != sysConfig.stop_after_alarm;
  bool baselineSampleChanged = temp.min_baseline_samples != sysConfig.min_baseline_samples;
  bool dirtySampleChanged = temp.dirty_timeout_samples != sysConfig.dirty_timeout_samples;

  bool currentChanged = temp.current_threshold != sysConfig.current_threshold;
  bool powerChanged = temp.power_threshold != sysConfig.power_threshold;

  sysConfig = temp;

  // ==== RUNTIME APPLY UPDATE ====

  // prox mode
  if (proxChanged) {

    setProximityActiveLow(sysConfig.prox_active_low);

    logToFile(
      "📡 Proximity mode updated: %s",
      sysConfig.prox_active_low ? "LOW" : "HIGH");
  }

  // buzzer dimatikan
  if (alarmChanged && !sysConfig.alarm_enabled) {

    sysStopAlarm();

    logToFile(
      "🚨 Alarm device updated: %s",
      sysConfig.alarm_enabled ? "LOW" : "HIGH");
  }

  // partition berubah
  if (partitionChanged) {

    DateTime now = getNow();

    resetRuntimePartitionStats(
      now.hour() / sysConfig.partition_hours);

    invalidateBaselineCache();

    logToFile(
      "📊 Partition stats reset. | updated: %u hours",
      sysConfig.partition_hours);
  }

  // record interval berubah
  if (intervalChanged) {

    logToFile(
      "⏱ Record interval updated: %u sec",
      sysConfig.record_interval_sec);
  }

  // estrus threshold berubah
  if (estrusThresholdChanged) {

    logToFile(
      "📈 Estrus threshold updated: %.1f%%",
      sysConfig.estrus_threshold_pct);
  }

  // retention days berubah
  if (retentionChanged) {

    invalidateBaselineCache();

    logToFile(
      "🗂 Retention days updated: %u days",
      sysConfig.retention_days);
  }

  // alarm behavior berubah
  if (stopAlarmChanged) {

    logToFile(
      "🔔 stop_after_alarm updated: %d",
      sysConfig.stop_after_alarm);
  }

  // min baseline samples berubah
  if (baselineSampleChanged) {

    invalidateBaselineCache();

    logToFile(
      "🔁 min_baseline_samples updated: %d",
      sysConfig.min_baseline_samples);
  }

  // dirty timeout samples berubah
  if (dirtySampleChanged) {

    resetDirtyDetection();

    logToFile(
      "🔁 dirty_timeout_samples updated: %d",
      sysConfig.dirty_timeout_samples);
  }

  // current threshold berubah
  if (currentChanged) {

    logToFile(
      "⚡ Current threshold updated: %.1f%%",
      sysConfig.current_threshold);
  }

  // power threshold berubah
  if (powerChanged) {

    logToFile(
      "⚡ Power threshold updated: %.1f%%",
      sysConfig.power_threshold);
  }

  // ==== SAVE CONFIG ====
  saveConfig();

  logToFile(
    "⚙️ CONFIG UPDATED");

  // ========================
  // RESPONSE
  // ========================
  String json = "{";

  json += "\"success\":true,";
  json += "\"restart\":";
  json += needRestart ? "true" : "false";
  json += ",";

  json += "\"message\":config updated";

  json += "}";

  server.send(
    200,
    "application/json",
    json);

  // ========================
  // RESTART
  // ========================
  if (needRestart) {

    pendingRestart = true;

    restartAt = millis() + 1500;
  }
}

void handleResetConfig() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  resetConfig();

  server.send(200, "application/json", "{\"reset\":true}");
}

// ===== HANDLE DEVICE NODE =====
void handleLatest() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  json += "\"node_id\":\"" + String(sysConfig.node_id) + "\",";
  json += "\"animal_id\":\"" + String(sysConfig.animal_id) + "\",";

  if (SYS.rtc_ok) {

    json += "\"time\":\"";
    json += nowStr();
    json += "\",";

  } else {

    json += "\"time\":\"invalid\",";
  }

  // ========================
  // ACTIVITY SENSOR
  // ========================
  json += "\"sensor1\":" + String(SYS.sensor1 ? 1 : 0) + ",";
  json += "\"sensor2\":" + String(SYS.sensor2 ? 1 : 0) + ",";
  json += "\"sensor1_dirty\":" + String(SYS.sensor1_dirty ? 1 : 0) + ",";
  json += "\"sensor2_dirty\":" + String(SYS.sensor2_dirty ? 1 : 0) + ",";

  // ========================
  // POWER
  // ========================
  json += "\"voltage\":" + String(SYS.voltage) + ",";
  json += "\"current\":" + String(SYS.current) + ",";
  json += "\"power\":" + String(SYS.power) + ",";

  // ========================
  // BATTERY
  // ========================
  json += "\"battery_percent\":" + String(SYS.battery_pct) + ",";
  json += "\"battery_days\":" + String(powerStats.estimated_days_left, 1) + ",";
  json += "\"battery_date\":\"" + String(powerStats.estimated_date) + "\",";

  // ========================
  // SYSTEM STATUS
  // ========================
  json += "\"sd\":" + String(SYS.sd_ok ? 1 : 0) + ",";
  json += "\"rtc\":" + String(SYS.rtc_ok ? 1 : 0) + ",";
  json += "\"sensor\":" + String(SYS.sensor_ok ? 1 : 0) + ",";
  json += "\"wifi\":" + String(wifiEnabled ? 1 : 0) + ",";
  json += "\"buzzer\":" + String(SYS.buzzer_active ? 1 : 0);

  json += "}";

  server.send(200, "application/json", json);
}

void handleEstrus() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  json += "\"partition\":";
  json += String(SYS.partition);
  json += ",";

  json += "\"current_rate\":";
  json += String(SYS.current_rate * 100.0f, 1);
  json += ",";

  json += "\"baseline_rate\":";
  json += String(SYS.baseline_rate * 100.0f, 1);
  json += ",";

  json += "\"deviation_pct\":";
  json += String(SYS.deviation_pct, 1);
  json += ",";

  json += "\"threshold_pct\":";
  json += String(sysConfig.estrus_threshold_pct, 1);
  json += ",";

  json += "\"baseline_samples\":";
  json += String(SYS.baseline_samples);
  json += ",";

  json += "\"estrus\":";
  json += String(SYS.estrus ? 1 : 0);
  json += ",";

  json += "\"valid\":";  // agar tahu baseline sudah cukup atau belum
  json += String(SYS.baseline_samples >= sysConfig.min_baseline_samples);

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}

void handleStorage() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  if (!SYS.sd_ok) {

    server.send(
      200,
      "application/json",
      "{\"sd\":false}");

    return;
  }

  uint64_t total =
    SD.totalBytes();

  uint64_t used =
    SD.usedBytes();

  String json = "{";

  json += "\"retention_days\":";
  json += String(
    sysConfig.retention_days);
  json += ",";

  json += "\"csv_rows_today\":";
  json += String(csvRowsWritten);
  json += ",";

  json += "\"free_sd_mb\":";
  json += String(
    (total - used) / 1024.0 / 1024.0,
    1);
  json += ",";

  json += "\"used_sd_mb\":";
  json += String(
    used / 1024.0 / 1024.0,
    1);
  json += ",";

  json += "\"log_queue\":";
  json += String(
    uxQueueMessagesWaiting(
      logQueue));
  json += ",";

  json += "\"sensor_queue\":";
  json += String(
    uxQueueMessagesWaiting(
      sensorQueue));

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}

void handleHealth() {

  // if (!isAuthenticated()) {

  //   server.send(
  //     401,
  //     "application/json",
  //     "{\"error\":\"unauthorized\"}");

  //   return;
  // }

  String json = "{";

  json += "\"sd\":";
  json += String(SYS.sd_ok ? "true" : "false");
  json += ",";

  json += "\"rtc\":";
  json += String(SYS.rtc_ok ? "true" : "false");
  json += ",";

  json += "\"sensor\":";
  json += String(SYS.sensor_ok ? "true" : "false");
  json += ",";

  json += "\"sensor_dirty\":";
  json += String(sysIsSensorDirty() ? "true" : "false");
  json += ",";

  json += "\"wifi\":";
  json += String(wifiEnabled ? "true" : "false");
  json += ",";

  json += "\"alarm\":";
  json += String(sysIsAlarm() ? "true" : "false");
  json += ",";

  json += "\"low_battery\":";
  json += String(sysIsLowBattery() ? "true" : "false");

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}

void handleDevice() {

  // if (!isAuthenticated()) {

  //   server.send(
  //     401,
  //     "application/json",
  //     "{\"error\":\"unauthorized\"}");

  //   return;
  // }

  uint64_t mac =
    ESP.getEfuseMac();

  char macStr[18];

  snprintf(
    macStr,
    sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",

    (uint8_t)(mac >> 40),
    (uint8_t)(mac >> 32),
    (uint8_t)(mac >> 24),
    (uint8_t)(mac >> 16),
    (uint8_t)(mac >> 8),
    (uint8_t)(mac));

  String json = "{";

  json += "\"node_id\":\"";
  json += String(sysConfig.node_id);
  json += "\",";

  json += "\"animal_id\":\"";
  json += String(sysConfig.animal_id);
  json += "\",";

  json += "\"ap_ssid\":\"";
  json += getAPSSID();
  json += "\",";

  json += "\"mac\":\"";
  json += macStr;
  json += "\",";

  json += "\"firmware\":\"";
  json += FIRMWARE_VERSION;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}


// ===== INIT WEBSERVER =====
void initWebServer() {
  // Collect header cookie
  const char* headerKeys[] = { "Cookie" };
  size_t headerKeysCount = sizeof(headerKeys) / sizeof(char*);
  server.collectHeaders(headerKeys, headerKeysCount);

  // api endpoint
  server.on("/", HTTP_GET, []() {
    handleFileRead("/index.html");
  });

  // AUTH
  server.on("/api/check", HTTP_GET, handleCheckAuth);
  server.on("/api/login", HTTP_GET, handleLogin);
  server.on("/api/logout", HTTP_GET, handleLogout);

  // DEVICE NODE
  server.on("/api/node/latest", HTTP_GET, handleLatest);    // snapshot hardware & health
  server.on("/api/node/estrus", HTTP_GET, handleEstrus);    // informasi model estrus
  server.on("/api/node/history", HTTP_GET, handleHistory);  // untuk melihat data csv
  server.on("/api/node/health", HTTP_GET, handleHealth);    // untuk cek kesehatan device
  server.on("/api/download", HTTP_GET, handleDownload);     // untuk download data csv

  // CONFIG
  server.on("/api/config", HTTP_GET, handleGetConfig);          // untuk load config dari esp
  server.on("/api/config", HTTP_POST, handleSetConfig);         // untuk ubah config
  server.on("/api/config/reset", HTTP_GET, handleResetConfig);  // untuk reset config (belum dipakai)

  // CONTROL
  server.on("/api/status/buzzer", HTTP_GET, handleStatusBuzzer);  // untuk cek status alarm
  server.on("/api/buzzer/stop", HTTP_POST, handleStopBuzzer);     // untuk tombol stop alarm

  // SYSTEM
  server.on("/api/system", HTTP_GET, handleSystemStatus);  // untuk cek kondisi device
  server.on("/api/storage", HTTP_GET, handleStorage);      // untuk cek kondisi SDCard

  server.on("/ping", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();

  logToFile("✅ WebServer Ready");
}

void handleWebServer() {
  server.handleClient();
}
