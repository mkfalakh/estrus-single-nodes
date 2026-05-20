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
  if (isAuthenticated()) {
    server.send(200, "application/json", "{\"auth\":true}");
  } else {
    server.send(401, "application/json", "{\"auth\":false}");
  }
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
    delay(300);
    server.send(200, "application/json", "{\"success\":false}");
    return;
  }

  String hashedInput = hashPassword(pass, SALT);

  // ❌ password salah
  if (hashedInput != HASHED_PASS) {
    logToFile("❌ Login gagal (password salah)");
    delay(300);
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

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  String json = "{";

  json += "\"error\":" + String(sysIsError()) + ",";
  json += "\"low_battery\":" + String(sysIsLowBattery()) + ",";
  json += "\"alarm\":" + String(sysIsAlarm());

  json += "}";

  server.send(200, "application/json", json);
}

void handleHistory() {

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  String date = server.arg("date");
  String filename = "/data/" + String(sysConfig.node_id) + "-" + date + ".csv";

  File file = SD.open(filename);
  if (!file) {
    server.send(200, "application/json", "{\"rows\":[],\"has_next\":false}");
    return;
  }

  const int LIMIT = 6;

  char lines[LIMIT][128];

  // use psram
  // char (*lines)[128] = (char (*)[128]) ps_malloc(LIMIT * 128);

  int count = readLastLines(file, lines, LIMIT);

  file.close();

  String json = "{\"rows\":[";

  for (int i = 0; i < count; i++) {
    json += "\"";
    json += lines[i];
    json += "\"";

    if (i < count - 1) json += ",";
  }

  json += "],\"has_next\":false}";

  server.send(200, "application/json", json);
}

// ===== HANDLE BUZZER =====
void handleStatusBuzzer() {

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  String json = "{";
  json += "\"buzzer\":" + String(SYS.buzzer_active) + ",";
  json += "\"interval\":" + String(sysConfig.interval_hours);
  json += "}";

  server.send(200, "application/json", json);
}

void handleStopBuzzer() {

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  if (sysIsAlarm()) {
    logToFile("🛑 BUZZER STOPPED BY USER");
  } else {
    logToFile("⚠️ STOP requested but buzzer already OFF");
  }

  sysStopAlarm();

  server.send(200, "application/json", "{\"success\":true}");
}

// ===== HANDLE DOWNLOAD BUTTON =====
void handleDownload() {

  if (!isAuthenticated()) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }

  if (!server.hasArg("date")) {
    server.send(400, "text/plain", "Missing date");
    return;
  }

  String date = server.arg("date");
  String filename = "/data/" + String(sysConfig.node_id) + "-" + date + ".csv";

  File file = SD.open(filename);
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=" + date + ".csv");

  server.streamFile(file, "text/csv");
  file.close();
}

// ===== GET CONFIG DARI MEMORY ESP =====
void handleGetConfig() {

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  String json = "{";

  json += "\"node_id\":\"";
  json += String(sysConfig.node_id);
  json += "\",";

  json += "\"prox_low\":";
  json += String(sysConfig.prox_active_low ? 1 : 0);
  json += ",";

  json += "\"interval\":";
  json += String(sysConfig.interval_hours);
  json += ",";

  json += "\"buzzer_enabled\":";
  json += String(sysConfig.buzzer_enabled ? 1 : 0);
  json += ",";

  json += "\"score\":";
  json += String(sysConfig.score_threshold, 2);
  json += ",";

  json += "\"ratio_trigger\":";
  json += String(sysConfig.ratio_trigger, 2);
  json += ",";

  json += "\"persist\":";
  json += String(sysConfig.persist_required);
  json += ",";

  json += "\"ema\":";
  json += String(sysConfig.ema_alpha, 2);
  json += ",";

  json += "\"activity_min\":";
  json += String(sysConfig.activity_min);
  json += ",";

  json += "\"balance_min\":";
  json += String(sysConfig.balance_min, 2);

  // json += "\"current\":" + String(sysConfig.current_threshold) + ",";
  // json += "\"power\":" + String(sysConfig.power_threshold) + ",";

  json += "}";

  server.send(200, "application/json", json);
}

// ===== SET/SAVE CONFIG DARI USER =====
void handleSetConfig() {

  // ========================
  // AUTH
  // ========================
  if (!isAuthenticated()) {

    server.send(
      401,
      "application/json",
      "{\"error\":\"unauthorized\"}");

    return;
  }

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

    // restart hanya jika berbeda
    if (id != String(sysConfig.node_id)) {

      if (!isValidNodeId(id)) {

        server.send(
          400,
          "application/json",
          "{\"error\":\"invalid node_id\"}");

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

      needRestart = true;

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
        "{\"error\":\"invalid prox_low\"}");

      return;
    }

    temp.prox_active_low = (v == "1");
  }

  // ========================
  // INTERVAL
  // ========================
  if (server.hasArg("interval")) {

    int v = server.arg("interval").toInt();

    if (v < 1 || v > 24) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid interval\"}");

      return;
    }

    temp.interval_hours = v;
  }

  // ========================
  // BUZZER
  // ========================
  if (server.hasArg("buzzer_enabled")) {

    String v = server.arg("buzzer_enabled");

    if (v != "0" && v != "1") {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid buzzer\"}");

      return;
    }

    temp.buzzer_enabled = (v == "1");
  }

  // ========================
  // SCORE
  // ========================
  if (server.hasArg("score")) {

    float v = server.arg("score").toFloat();

    if (v < 0.1 || v > 5.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid score\"}");

      return;
    }

    temp.score_threshold = v;
  }

  // ========================
  // RATIO
  // ========================
  if (server.hasArg("ratio_trigger")) {

    float v = server.arg("ratio_trigger").toFloat();

    if (v < 0.1 || v > 10.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid ratio_trigger\"}");

      return;
    }

    temp.ratio_trigger = v;
  }

  // ========================
  // PERSIST
  // ========================
  if (server.hasArg("persist")) {

    int v = server.arg("persist").toInt();

    if (v < 1 || v > 20) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid persist\"}");

      return;
    }

    temp.persist_required = v;
  }

  // ========================
  // EMA
  // ========================
  if (server.hasArg("ema")) {

    float v = server.arg("ema").toFloat();

    if (v <= 0.0 || v > 1.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid ema\"}");

      return;
    }

    temp.ema_alpha = v;
  }

  // ========================
  // ACTIVITY MIN
  // ========================
  if (server.hasArg("activity_min")) {

    int v = server.arg("activity_min").toInt();

    if (v < 1 || v > 10000) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid activity_min\"}");

      return;
    }

    temp.activity_min = v;
  }

  // ========================
  // BALANCE
  // ========================
  if (server.hasArg("balance_min")) {

    float v = server.arg("balance_min").toFloat();

    if (v < 0.01 || v > 1.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid balance_min\"}");

      return;
    }

    temp.balance_min = v;
  }

  // ========================
  // ALL VALID → APPLY
  // ========================
  sysConfig = temp;

  // runtime update
  setProximityActiveLow(
    sysConfig.prox_active_low);

  // save prefs
  saveConfig();

  logToFile(
    "⚙️ CONFIG UPDATED");

  // ========================
  // RESPONSE
  // ========================
  String json = "{";

  json += "\"success\":true,";
  json += "\"restart\":";
  json += String(needRestart ? 1 : 0);

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

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  resetConfig();

  server.send(200, "application/json", "{\"reset\":true}");
}

// ===== HANDLE LATEST SYSTEM =====
void handleLatest() {

  if (!isAuthenticated()) {
    server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return;
  }

  String json = "{";

  json += "\"node_id\":\"" + String(sysConfig.node_id) + "\",";
  json += "\"time\":\"" + nowStr() + "\",";

  // ========================
  // POWER
  // ========================
  json += "\"voltage\":" + String(SYS.voltage) + ",";
  json += "\"current\":" + String(SYS.current) + ",";
  json += "\"power\":" + String(SYS.power) + ",";

  // ========================
  // ACTIVITY
  // ========================
  json += "\"a1\":" + String(SYS.a1) + ",";
  json += "\"a2\":" + String(SYS.a2) + ",";
  json += "\"total\":" + String(SYS.total) + ",";

  // ========================
  // MODEL
  // ========================
  json += "\"score\":" + String(SYS.score, 2) + ",";
  json += "\"estrus\":" + String(SYS.estrus) + ",";

  // ========================
  // BATTERY
  // ========================
  json += "\"battery_percent\":" + String(SYS.battery_pct) + ",";
  json += "\"battery_days\":" + String(powerStats.estimated_days_left, 1) + ",";
  json += "\"battery_date\":\"" + String(powerStats.estimated_date) + "\",";

  // ========================
  // SYSTEM STATUS
  // ========================
  json += "\"sd\":" + String(SYS.sd_ok) + ",";
  json += "\"rtc\":" + String(SYS.rtc_ok) + ",";
  json += "\"sensor\":" + String(SYS.sensor_ok) + ",";
  json += "\"buzzer\":" + String(SYS.buzzer_active);

  json += "}";

  server.send(200, "application/json", json);
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

  server.on("/api/check", HTTP_GET, handleCheckAuth);
  server.on("/api/login", HTTP_GET, handleLogin);
  server.on("/api/logout", HTTP_GET, handleLogout);

  server.on("/api/node/latest", HTTP_GET, handleLatest);
  server.on("/api/node/history", HTTP_GET, handleHistory);
  server.on("/api/download", HTTP_GET, handleDownload);

  server.on("/api/config/get", HTTP_GET, handleGetConfig);
  server.on("/api/config/set", HTTP_GET, handleSetConfig);
  server.on("/api/config/reset", HTTP_GET, handleResetConfig);
  server.on("/api/status/buzzer", HTTP_GET, handleStatusBuzzer);
  server.on("/api/buzzer/stop", HTTP_GET, handleStopBuzzer);
  server.on("/api/system", HTTP_GET, handleSystemStatus);

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
