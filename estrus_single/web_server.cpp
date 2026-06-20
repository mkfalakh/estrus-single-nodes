#include "http_parser.h"
#include "web_server.h"
#include "auth.h"
#include "crypto.h"
#include "config.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "csv_reverse.h"
#include "buzzer.h"
#include "config_runtime.h"
#include "power_monitor.h"
#include "system_state.h"
#include "logger.h"
#include "sens_proximity.h"
#include "storage_stats.h"
#include "estrus_model.h"
#include "wifi_manager.h"
#include "csv_writer.h"
#include <WebServer.h>
#include <SD.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#define TZ_OFFSET (7 * 3600)  // timezone GMT+7 WIB

static void logRequest();
static void logResponse(int code, const char* note = nullptr);

WebServer server(80);

// update last client
inline void touchClient() {
  lastClientTime = millis();
}

// String jsonEscape(const String& s) {

//   String out;

//   for (char c : s) {

//     if (c == '\"')
//       out += '\\';

//     out += c;
//   }

//   return out;
// }

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

// kolom sesuai header CSV di csv_writer.cpp
static const char* CSV_COLUMNS[] = {
  "device_id",
  "animal_id",
  "timestamp",
  "sensor1_state",
  "sensor2_state",
  "sensor1_dirty",
  "sensor2_dirty",
  "deviation",
  "estrus",
  "voltage",
  "current",
  "battery_pct"
};
static const int CSV_COLUMN_COUNT = sizeof(CSV_COLUMNS) / sizeof(CSV_COLUMNS[0]);

// fields yang ditulis sebagai string di JSON (sisanya angka/bool, tanpa quote)
static bool isCsvStringField(const char* col) {
  return strcmp(col, "device_id") == 0
         || strcmp(col, "animal_id") == 0
         || strcmp(col, "timestamp") == 0;
}

// ubah satu baris CSV ("a,b,c,...") menjadi objek JSON {"device_id":"a",...}
static String csvRowToJson(const char* line) {

  String json = "{";

  int col = 0;
  int start = 0;
  int len = strlen(line);

  for (int i = 0; i <= len && col < CSV_COLUMN_COUNT; i++) {

    if (i == len || line[i] == ',') {

      String value = String(line).substring(start, i);

      if (col > 0) json += ",";

      json += "\"";
      json += CSV_COLUMNS[col];
      json += "\":";

      if (isCsvStringField(CSV_COLUMNS[col])) {
        json += "\"";
        json += escapeJson(value.c_str());
        json += "\"";
      } else if (value.length() == 0) {
        json += "0";
      } else {
        json += value;
      }

      col++;
      start = i + 1;
    }
  }

  json += "}";

  return json;
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
  logResponse(200);
}

void handleLogout() {
  logToFile("✅ Logout sukses");
  server.sendHeader("Set-Cookie", "ESPSESSIONID=deleted; Path=/; Max-Age=0");
  server.send(200, "application/json", "{\"success\":true}");
  logResponse(200);
}

void handleLogin() {

  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"missing user or pass\"}");
    logResponse(400, "missing args");
    return;
  }

  String user = server.arg("user");
  String pass = server.arg("pass");

  // ❌ user salah
  if (user != USER) {
    logToFile("❌ Login gagal (user salah): " + user);
    // vTaskDelay(pdMS_TO_TICKS(300));
    server.send(200, "application/json", "{\"success\":false}");
    logResponse(200, "wrong user");
    return;
  }

  String hashedInput = hashPassword(pass, SALT);

  // ❌ password salah
  if (hashedInput != HASHED_PASS) {
    logToFile("❌ Login gagal (password salah)");
    // vTaskDelay(pdMS_TO_TICKS(300));
    server.send(200, "application/json", "{\"success\":false}");
    logResponse(200, "wrong pass");
    return;
  }

  // ✅ login berhasil
  String sessionId;
  if (!createSession(sessionId)) {
    server.send(500, "application/json", "{\"error\":\"session penuh\"}");
    logResponse(500, "session full");
    return;
  }

  logToFile("✅ Login sukses: " + user);

  server.sendHeader("Set-Cookie", "ESPSESSIONID=" + sessionId + "; Path=/; SameSite=Lax; Max-Age=3600");

  server.send(200, "application/json", "{\"success\":true}");
  logResponse(200, "ok");
}


void handleHistory() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  touchClient();

  // LIMIT AMBIL DATA CSV DI APP/DASHBOARD
  constexpr int DEFAULT_LIMIT = 10;
  constexpr int MAX_LIMIT = 20;
  constexpr int MAX_PAGE = 20;

  String date = server.arg("date");

  logToFile("📜 [history] request date=" + date);

  if (date.length() != 10 || date[4] != '-' || date[7] != '-') {

    logToFile("❌ [history] invalid date: " + date);

    server.send(
      400,
      "application/json",
      "{\"error\":\"invalid date\",\"field\":\"date\",\"reason\":\"must be YYYY-MM-DD\"}");

    logResponse(400, "invalid date");

    return;
  }

  if (!takeSDMutex("HISTORY", pdMS_TO_TICKS(3000))) {

    server.send(
      503,
      "application/json",
      "{\"error\":\"sd_busy\"}");

    logResponse(503, "sd_busy");

    return;
  }

  int page = 0;
  int limit = DEFAULT_LIMIT;

  String filename = "/data/" + date + ".csv";

  if (!SD.exists(filename)) {

    logToFile("⚠️ [history] file not found: " + filename);

    giveSDMutex();

    server.send(
      200,
      "application/json",
      "{\"date\":\"" + date + "\",\"rows\":[],\"has_next\":false}");

    logResponse(200, "file not found, empty rows");

    return;
  }

  File file = SD.open(filename);

  if (!file) {

    logToFile("❌ [history] failed to open: " + filename);

    sysSetSD(false);

    giveSDMutex();

    server.send(
      500,
      "application/json",
      "{\"error\":\"sd\"}");

    logResponse(500, "sd open failed");

    return;
  }

  sysSetSD(true);

  // PAGE
  if (server.hasArg("page")) {

    page = (int)server.arg("page").toInt();

    if (page < 0) {
      page = 0;
    }

    page = constrain(
      page,
      0,
      MAX_PAGE);
  }

  // LIMIT
  if (server.hasArg("limit")) {

    long l = server.arg("limit").toInt();

    limit = constrain(
      l,
      1L,
      (long)MAX_LIMIT);
  }

  // char lines[LIMIT][160]; // alternatif jika PSRAM tidak bisa

  // coba PSRAM dulu, fallback ke heap biasa jika PSRAM tidak tersedia
  char(*lines)[160] =
    (char(*)[160])ps_malloc(
      limit * sizeof(*lines));

  if (!lines) {

    logToFile("⚠️ [history] ps_malloc failed, fallback to malloc, limit=" + String(limit));

    lines = (char(*)[160])malloc(limit * sizeof(*lines));
  }

  if (!lines) {

    logToFile("❌ [history] malloc failed, limit=" + String(limit));

    file.close();

    giveSDMutex();

    server.send(
      500,
      "application/json",
      "{\"error\":\"oom\"}");

    logResponse(500, "oom");

    return;
  }

  bool hasNext = false;

  taskYIELD();

  int count =
    readCsvPage(
      file,
      lines,
      page,
      limit,
      hasNext);

  file.close();

  giveSDMutex();

  logToFile("✅ [history] date=" + date + " page=" + String(page) + " limit=" + String(limit) + " count=" + String(count) + " hasNext=" + String(hasNext ? "true" : "false"));

  String json;

  if (!json.reserve(256 + (count * 180))) {
    free(lines);
    server.send(500, "application/json", "{\"error\":\"oom_json\"}");
    logResponse(500, "oom json reserve");
    return;
  }

  json = "{";

  json += "\"date\":\"";
  json += date;
  json += "\",";

  json += "\"page\":";
  json += String(page);

  json += ",\"limit\":";
  json += String(limit);

  json += ",\"count\":";
  json += String(count);

  json += ",\"rows\":[";

  // for (int i = 0; i < count; i++) {
  //   json += csvRowToJson(lines[i]);

  //   if (i < count - 1) json += ",";
  // }

  for (int i = 0; i < count; i++) {

    json += csvRowToJson(lines[i]);

    if (i < count - 1) {
      json += ",";
    }

    if ((i % 10) == 0) {
      taskYIELD();
    }
  }

  json += "],";

  json += "\"has_next\":";
  json += String(
    hasNext ? "true" : "false");

  json += "}";


  free(lines);

  server.sendHeader("Connection", "close");
  server.send(200, "application/json", json);
  logResponse(200);
}

// ===== HANDLE ALARM =====
void handleAlarmStatus() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  String json = "{";

  json += "\"alarm_active\":" + String(sysIsAlarm() ? "true" : "false") + ",";
  json += "\"alarm_estrus\":" + String(SYS.estrus ? "true" : "false") + ",";
  json += "\"alarm_fault\":" + String(isFaultAlarm() ? "true" : "false") + ",";
  json += "\"alarm_fault_muted\":" + String(SYS.fault_alarm_muted ? "true" : "false") + ",";
  json += "\"alarm_ack\":" + String(isAlarmAcknowledged() ? "true" : "false") + ",";
  json += "\"stop_after_alarm\":" + String(sysConfig.stop_after_alarm ? "true" : "false");

  json += "}";

  server.send(200, "application/json", json);
  logResponse(200);
}

void handleAlarmStop() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  if (sysConfig.stop_after_alarm) {

    acknowledgeAlarm();

  } else {

    sysStopAlarm();
  }

  logToFile("🔕 Alarm stopped via API");

  server.send(200, "application/json", "{\"success\":true}");
  logResponse(200);
}

void handleAlarmStart() {

  SYS.alarm_ack = false;

  SYS.fault_alarm_muted = false;

  logToFile("🔔 Alarm resume via API");

  server.send(200, "application/json", "{\"success\":true}");
  logResponse(200);
}

// ===== HANDLE DOWNLOAD BUTTON =====

// Returns true if columns 0 (device_id) and 1 (animal_id) match sysConfig.
// Uses byte-by-byte compare — ROM strncmp uses L32I word loads and crashes
// with LoadStoreAlignment when the pointer is not 4-byte aligned.
static bool matchesDeviceFilter(const char *line) {
  int nlen = strlen(sysConfig.node_id);
  int alen = strlen(sysConfig.animal_id);

  const char *p = line;

  // field 0: device_id
  if (nlen > 0) {
    for (int i = 0; i < nlen; i++, p++) {
      if (*p != sysConfig.node_id[i]) return false;
    }
    if (*p != ',') return false;
    p++;
  } else {
    while (*p && *p != ',') p++;
    if (!*p) return false;
    p++;
  }

  // field 1: animal_id
  if (alen > 0) {
    for (int i = 0; i < alen; i++, p++) {
      if (*p != sysConfig.animal_id[i]) return false;
    }
    if (*p != ',' && *p != '\0') return false;
  }

  return true;
}

// Extract "YYYY-MM-DD HH:MM" minute-key from a CSV line.
// CSV format: device_id,animal_id,YYYY-MM-DD HH:MM:SS,...
static bool extractMinuteKey(const char *line, char out[17]) {
  int commas = 0;
  int i = 0;
  while (line[i] && commas < 2) {
    if (line[i] == ',') commas++;
    i++;
  }
  if (strlen(line + i) < 16) return false;
  memcpy(out, line + i, 16);
  out[16] = '\0';
  return true;
}

// Streams all CSV files within the configured retention window,
// filtered to the latest record per minute (chronological, oldest first).
// No ?date= param needed — uses RTC now and sysConfig.retention_days.
void handleDownload() {

  if (!SYS.rtc_ok) {
    server.send(503, "text/plain", "RTC not ready");
    logResponse(503, "rtc not ready");
    return;
  }

  if (!SYS.sd_ok) {
    server.send(503, "text/plain", "SD not available");
    logResponse(503, "sd not available");
    return;
  }

  DateTime now = getNow();

  String csvName = String(sysConfig.node_id) + "-retention.csv";

  server.sendHeader("Content-Disposition", "attachment; filename=" + csvName);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  logResponse(200, "streaming csv");

  server.sendContent(
    "device_id,animal_id,timestamp,sensor1_state,sensor2_state,"
    "sensor1_dirty,sensor2_dirty,deviation,estrus,voltage,current,battery_pct\r\n");

  // Send buffer: collect per-minute winners while holding SD mutex,
  // then release mutex and do HTTP I/O without holding it.
  static const int SEND_BUF_LINES = 32;
  char sendBuf[SEND_BUF_LINES][160];
  int  sendCount = 0;

  char lineBuf[160];
  char prevLine[160];
  char prevMinKey[17];
  char curMinKey[17];

  // iterate oldest → newest within retention window
  for (int d = (int)sysConfig.retention_days - 1; d >= 0; d--) {

    DateTime day(now.unixtime() - (uint32_t)d * 86400UL);

    char dateStr[11];
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
             day.year(), day.month(), day.day());

    String filename = "/data/" + String(dateStr) + ".csv";

    bool fileDone = false;
    long filePos  = 0;

    // Read file in chunks: take mutex → read batch → give mutex → send batch
    while (!fileDone) {

      if (!takeSDMutex("DOWNLOAD", pdMS_TO_TICKS(3000))) break;

      File file = SD.open(filename);
      if (!file) {
        sysSetSD(false);
        giveSDMutex();
        break;
      }
      sysSetSD(true);

      if (filePos == 0) {
        prevLine[0]   = '\0';
        prevMinKey[0] = '\0';
      }
      file.seek(filePos);

      sendCount = 0;
      bool firstLine = (filePos == 0);

      // Read up to SEND_BUF_LINES winners per mutex hold
      while (file.available() && sendCount < SEND_BUF_LINES) {

        int len = file.readBytesUntil('\n', lineBuf, sizeof(lineBuf) - 1);
        lineBuf[len] = '\0';

        if (len > 0 && lineBuf[len - 1] == '\r') lineBuf[--len] = '\0';

        if (firstLine) { firstLine = false; continue; }  // skip CSV header row
        if (len == 0) continue;

        if (!matchesDeviceFilter(lineBuf)) continue;
        if (!extractMinuteKey(lineBuf, curMinKey)) continue;

        if (strcmp(curMinKey, prevMinKey) != 0) {
          if (prevLine[0] != '\0') {
            strncpy(sendBuf[sendCount], prevLine, 159);
            sendBuf[sendCount][159] = '\0';
            sendCount++;
          }
          strncpy(prevMinKey, curMinKey, sizeof(prevMinKey));
        }

        strncpy(prevLine, lineBuf, sizeof(prevLine) - 1);
        prevLine[sizeof(prevLine) - 1] = '\0';
      }

      fileDone = !file.available();
      filePos  = file.position();

      // flush final record on last chunk
      if (fileDone && prevLine[0] != '\0' && sendCount < SEND_BUF_LINES) {
        strncpy(sendBuf[sendCount], prevLine, 159);
        sendBuf[sendCount][159] = '\0';
        sendCount++;
        prevLine[0] = '\0';
      }

      file.close();
      giveSDMutex();

      // Send buffered lines without holding SD mutex
      for (int i = 0; i < sendCount; i++) {
        server.sendContent(sendBuf[i]);
        server.sendContent("\r\n");
      }

      // yield so logger and csv_writer can get the mutex
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
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

  json += "\"min_baseline_windows\":";
  json += String(sysConfig.min_baseline_windows);
  json += ",";

  json += "\"dirty_timeout_min\":";
  json += String(sysConfig.dirty_timeout_min);
  json += ",";

  // BATTERY
  json += "\"current_threshold\":" + String(sysConfig.current_threshold) + ",";
  json += "\"power_threshold\":" + String(sysConfig.power_threshold) + ",";

  // Hormone injection date. Estrus typically shows ~day 20-21 from injection.
  json += "\"injection_date\":\"";
  json += String(sysConfig.injection_date);
  json += "\"";

  json += "}";

  server.send(200, "application/json", json);
  logResponse(200);
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
  // PARSE JSON BODY
  // ========================
  String body = server.hasArg("plain") ? server.arg("plain") : "";

  logToFile("📝 [config] POST /api/config body=" + body);

  DynamicJsonDocument doc(1536);

  if (body.length() > 0) {

    DeserializationError err = deserializeJson(doc, body);

    if (err) {

      logToFile("❌ [config] invalid JSON: %s", err.c_str());

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid json body\",\"reason\":\"body is not valid JSON\"}");

      logResponse(400, "invalid json");

      return;
    }
  }

  // helper: cek key ada di JSON body
  auto hasField = [&](const char* key) {
    return doc.containsKey(key);
  };

  // helper: ambil value sebagai String
  auto fieldStr = [&](const char* key) {
    return String((const char*)doc[key]);
  };

  // ========================
  // TEMP CONFIG
  // ========================
  SystemConfig temp = sysConfig;

  bool needRestart = false;

  // ========================
  // NODE ID
  // ========================
  if (hasField("node_id")) {

    String id = fieldStr("node_id");

    id.trim();

    // restart hanya jika berbeda = true
    if (id != String(sysConfig.node_id)) {

      if (!isValidNodeId(id)) {

        server.send(
          400,
          "application/json",
          "{\"error\":\"invalid node_id cfg\",\"field\":\"node_id\",\"reason\":\"failed isValidNodeId check\"}");

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
  if (hasField("prox_low")) {

    int v = doc["prox_low"].as<int>();

    if (v != 0 && v != 1) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid prox_low cfg\",\"field\":\"prox_low\",\"reason\":\"must be 0 or 1\"}");

      return;
    }

    temp.prox_active_low = (v == 1);
  }

  // ========================
  // BUZZER/ALARM
  // ========================
  if (hasField("alarm_enabled")) {

    int v = doc["alarm_enabled"].as<int>();

    if (v != 0 && v != 1) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid alarm cfg\",\"field\":\"alarm_enabled\",\"reason\":\"must be 0 or 1\"}");

      return;
    }

    temp.alarm_enabled = (v == 1);
  }

  // ========================
  // ANIMAL ID
  // ========================
  if (hasField("animal_id")) {

    String id = fieldStr("animal_id");

    id.trim();

    // restart hanya jika berbeda = false
    if (id != String(sysConfig.animal_id)) {

      if (!isValidAnimalId(id)) {

        server.send(
          400,
          "application/json",
          "{\"error\":\"invalid animal_id cfg\",\"field\":\"animal_id\",\"reason\":\"failed isValidAnimalId check\"}");

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
  if (hasField("ap_password")) {

    String pass =
      fieldStr("ap_password");

    pass.trim();

    if (
      pass.length() < 8 || pass.length() > 20) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid ap_password\",\"field\":\"ap_password\",\"reason\":\"length must be 8-20 chars\"}");

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
  if (hasField("record_interval_sec")) {

    int v = doc["record_interval_sec"].as<int>();

    if (v < 10 || v > 3600) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid record_interval_sec cfg\",\"field\":\"record_interval_sec\",\"reason\":\"must be 10-3600\"}");

      return;
    }

    temp.record_interval_sec = v;
  }

  // ========================
  // RETENTION DAYS
  // ========================
  if (hasField("retention_days")) {

    int v = doc["retention_days"].as<int>();

    if (v < 1 || v > 21) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid retention_days cfg\",\"field\":\"retention_days\",\"reason\":\"must be 1-21\"}");

      return;
    }

    temp.retention_days = v;
  }

  // ========================
  // PARTITION HOURS
  // ========================
  if (hasField("partition_hours")) {

    int v = doc["partition_hours"].as<int>();

    if (v < 3 || v > 24 || (24 % v) != 0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid partition_hours cfg\",\"field\":\"partition_hours\",\"reason\":\"must be 3-24 and a divisor of 24\"}");

      return;
    }

    temp.partition_hours = v;
  }

  // ========================
  // ESTRUS THRESHOLD PCT
  // ========================
  if (hasField("estrus_threshold_pct")) {

    float v = doc["estrus_threshold_pct"].as<float>();

    if (v < 0.0 || v > 100.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid estrus_threshold_pct cfg\",\"field\":\"estrus_threshold_pct\",\"reason\":\"must be 0.0-100.0\"}");

      return;
    }

    temp.estrus_threshold_pct = v;
  }

  // ========================
  // STOP AFTER ALARM
  // ========================
  if (hasField("stop_after_alarm")) {

    int v = doc["stop_after_alarm"].as<int>();

    if (v != 0 && v != 1) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid stop_after_alarm cfg\",\"field\":\"stop_after_alarm\",\"reason\":\"must be 0 or 1\"}");

      return;
    }

    temp.stop_after_alarm = (v == 1);
  }

  // ========================
  // MIN BASELINE WINDOWS
  // ========================
  if (hasField("min_baseline_windows")) {

    int v = doc["min_baseline_windows"].as<int>();

    if (v < 2 || v > 48) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid min_baseline_windows cfg\",\"field\":\"min_baseline_windows\",\"reason\":\"must be 2-48\"}");

      return;
    }

    temp.min_baseline_windows = (uint8_t)v;
  }

  // ========================
  // DIRTY TIMEOUT MIN
  // ========================
  if (hasField("dirty_timeout_min")) {

    int v = doc["dirty_timeout_min"].as<int>();

    if (v < 10 || v > 480) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid dirty_timeout_min cfg\",\"field\":\"dirty_timeout_min\",\"reason\":\"must be 10-480\"}");

      return;
    }

    temp.dirty_timeout_min = (uint16_t)v;
  }

  // ========================
  // START DATE (YYYY-MM-DD)
  // Hormone injection date used to synchronize or shorten the natural 21-day
  // reproductive cycle. Estrus typically shows ~day 20-21 from injection.
  // Send empty string to clear.
  // ========================
  if (hasField("injection_date")) {

    String id = fieldStr("injection_date");

    id.trim();

    if (id.length() == 0) {

      memset(temp.injection_date, 0, sizeof(temp.injection_date));

    } else if (
      id.length() == 10
      && id[4] == '-'
      && id[7] == '-') {

      memset(temp.injection_date, 0, sizeof(temp.injection_date));

      strncpy(temp.injection_date, id.c_str(), sizeof(temp.injection_date) - 1);

    } else {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid injection_date\",\"field\":\"injection_date\",\"reason\":\"must be YYYY-MM-DD or empty string\"}");

      logResponse(400, "invalid injection_date");

      return;
    }
  }

  // ========================
  // POWER & CURRENT BATTERY
  // ========================
  if (hasField("current_threshold")) {

    float v = doc["current_threshold"].as<float>();

    if (v < 100.0 || v > 150.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid current_threshold cfg\",\"field\":\"current_threshold\",\"reason\":\"must be 100.0-150.0\"}");

      return;
    }

    temp.current_threshold = v;
  }

  // POWER
  if (hasField("power_threshold")) {

    float v = doc["power_threshold"].as<float>();

    if (v < 400.0 || v > 600.0) {

      server.send(
        400,
        "application/json",
        "{\"error\":\"invalid power_threshold cfg\",\"field\":\"power_threshold\",\"reason\":\"must be 400.0-600.0\"}");

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
  bool baselineWindowChanged = temp.min_baseline_windows != sysConfig.min_baseline_windows;
  bool dirtyMinChanged       = temp.dirty_timeout_min   != sysConfig.dirty_timeout_min;

  bool currentChanged = temp.current_threshold != sysConfig.current_threshold;
  bool powerChanged = temp.power_threshold != sysConfig.power_threshold;
  bool injectionDateChanged = strcmp(temp.injection_date, sysConfig.injection_date) != 0;

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

  // min baseline windows berubah
  if (baselineWindowChanged) {

    logToFile(
      "🔁 min_baseline_windows updated: %d",
      sysConfig.min_baseline_windows);
  }

  // dirty timeout min berubah
  if (dirtyMinChanged) {

    resetDirtyDetection();

    logToFile(
      "🔁 dirty_timeout_min updated: %d min",
      sysConfig.dirty_timeout_min);
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

  // injection date berubah
  if (injectionDateChanged) {

    logToFile(
      "📅 Injection date updated: %s",
      sysConfig.injection_date[0] ? sysConfig.injection_date : "(cleared)");
  }

  // ==== SAVE CONFIG ====
  saveConfig();

  // ========================
  // LOG SAVED CONFIG (JSON)
  // ========================
  {
    String savedJson = "{";

    savedJson += "\"node_id\":\"" + String(sysConfig.node_id) + "\",";
    savedJson += "\"animal_id\":\"" + String(sysConfig.animal_id) + "\",";
    savedJson += "\"ap_password\":\"" + String(sysConfig.ap_password) + "\",";
    savedJson += "\"prox_low\":" + String(sysConfig.prox_active_low ? 1 : 0) + ",";
    savedJson += "\"alarm_enabled\":" + String(sysConfig.alarm_enabled ? 1 : 0) + ",";
    savedJson += "\"record_interval_sec\":" + String(sysConfig.record_interval_sec) + ",";
    savedJson += "\"retention_days\":" + String(sysConfig.retention_days) + ",";
    savedJson += "\"partition_hours\":" + String(sysConfig.partition_hours) + ",";
    savedJson += "\"estrus_threshold_pct\":" + String(sysConfig.estrus_threshold_pct, 2) + ",";
    savedJson += "\"stop_after_alarm\":" + String(sysConfig.stop_after_alarm ? 1 : 0) + ",";
    savedJson += "\"min_baseline_windows\":" + String(sysConfig.min_baseline_windows) + ",";
    savedJson += "\"dirty_timeout_min\":" + String(sysConfig.dirty_timeout_min) + ",";
    savedJson += "\"current_threshold\":" + String(sysConfig.current_threshold) + ",";
    savedJson += "\"power_threshold\":" + String(sysConfig.power_threshold) + ",";
    savedJson += "\"injection_date\":\"" + String(sysConfig.injection_date) + "\"";

    savedJson += "}";

    logToFile("💾 [config] saved=" + savedJson);
  }

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

  json += "\"message\":\"config updated\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json);

  logResponse(200, needRestart ? "restart pending" : "ok");

  // ========================
  // DEFERRED HEAVY WORK (after response sent to avoid client timeout)
  // ========================
  if (partitionChanged || retentionChanged || baselineWindowChanged) {
    triggerBaselineRecompute();
  }

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
  logResponse(200);
}

// ===== HANDLE DEVICE NODE =====
void handleLatest() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  touchClient();

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
  auto safeFloat = [](float v) -> String {
    if (isnan(v) || isinf(v)) return "0.00";
    return String(v, 2);
  };

  json += "\"voltage\":"         + safeFloat(SYS.voltage)      + ",";
  json += "\"current\":"         + safeFloat(SYS.current)      + ",";
  json += "\"power\":"           + safeFloat(SYS.power)        + ",";

  // ========================
  // BATTERY
  // ========================
  json += "\"battery_percent\":" + safeFloat(SYS.battery_pct)  + ",";

  float _bdays = powerStats.estimated_days_left;
  json += "\"battery_days\":"    + (isnan(_bdays) || isinf(_bdays) ? String("0.0") : String(_bdays, 1)) + ",";
  json += "\"battery_date\":\"" + String(powerStats.estimated_date) + "\",";

  // ========================
  // SYSTEM STATUS
  // ========================
  json += "\"sd\":" + String(SYS.sd_ok ? 1 : 0) + ",";
  json += "\"rtc\":" + String(SYS.rtc_ok ? 1 : 0) + ",";
  json += "\"sensor\":" + String(SYS.sensor_ok ? 1 : 0) + ",";
  json += "\"wifi\":" + String(wifiEnabled ? 1 : 0) + ",";
  json += "\"buzzer\":" + String(SYS.alarm_active ? 1 : 0) + ",";
  json += "\"sensor_dirty\":" + String(sysIsSensorDirty() ? 1 : 0) + ",";
  json += "\"alarm\":" + String(sysIsAlarm() ? 1 : 0) + ",";
  json += "\"low_battery\":" + String(sysIsLowBattery() ? 1 : 0);

  json += "}";

  server.sendHeader("Connection", "close");

  server.send(200, "application/json", json);
  logResponse(200);
}

void handleEstrus() {

  // if (!isAuthenticated()) {
  //   server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
  //   return;
  // }

  touchClient();

  // --- cycle day from injection_date ---
  int cycleDay = 0;
  bool hasInjectionDate = (strlen(sysConfig.injection_date) == 10);

  if (hasInjectionDate && SYS.rtc_ok) {

    DateTime now = getNow();

    // parse YYYY-MM-DD
    char buf[5];
    strncpy(buf, sysConfig.injection_date, 4); buf[4] = '\0';
    int sy = atoi(buf);
    strncpy(buf, sysConfig.injection_date + 5, 2); buf[2] = '\0';
    int sm = atoi(buf);
    strncpy(buf, sysConfig.injection_date + 8, 2); buf[2] = '\0';
    int sd = atoi(buf);

    DateTime startDt(sy, sm, sd, 0, 0, 0);

    int32_t diffSec = (int32_t)(now.unixtime() - startDt.unixtime());

    if (diffSec >= 0) {
      cycleDay = (diffSec / 86400) + 1;
    }
  }

  bool isEstrusWindow = hasInjectionDate && (cycleDay >= 18 && cycleDay <= 21);

  String json = "{";

  json += "\"partition\":";
  json += String(SYS.partition);
  json += ",";

  auto safeFloat1 = [](float v) -> String {
    if (isnan(v) || isinf(v)) return "0.0";
    return String(v, 1);
  };

  json += "\"current_rate\":";
  json += safeFloat1(SYS.current_rate * 100.0f);
  json += ",";

  json += "\"baseline_rate\":";
  json += safeFloat1(SYS.baseline_rate * 100.0f);
  json += ",";

  json += "\"deviation_pct\":";
  json += safeFloat1(SYS.deviation_pct);
  json += ",";

  json += "\"threshold_pct\":100.0,";

  json += "\"baseline_windows\":";
  json += String(SYS.baseline_windows);
  json += ",";

  json += "\"estrus\":";
  json += String(SYS.estrus ? 1 : 0);
  json += ",";

  json += "\"valid\":";
  json += (SYS.baseline_windows >= sysConfig.min_baseline_windows) ? "true" : "false";
  json += ",";

  json += "\"injection_date\":\"";
  json += String(sysConfig.injection_date);
  json += "\",";

  json += "\"cycle_day\":";
  json += String(cycleDay);
  json += ",";

  // is_estrus_window = 1 jika berada di hari 18-21 dari siklus (window deteksi estrus sapi)
  json += "\"is_estrus_window\":";
  json += String(isEstrusWindow ? 1 : 0);
  json += ",";

  json += "\"window_count\":";
  json += String(getSlidingWindowCount());
  json += ",";

  json += "\"window_size\":";
  json += String(getSlidingWindowSize());

  json += "}";

  server.sendHeader("Connection", "close");

  server.send(
    200,
    "application/json",
    json);
  logResponse(200);
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

    logResponse(200, "sd not ok");

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

  server.sendHeader("Connection", "close");

  server.send(
    200,
    "application/json",
    json);
  logResponse(200);
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
  json += ",";

  json += "\"error\":";
  json += String(sysIsSystemFault() ? "true" : "false");

  json += "}";

  server.send(
    200,
    "application/json",
    json);
  logResponse(200);
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
  logResponse(200);
}

// POST RTC
void handleRTCSync() {

  StaticJsonDocument<128> doc;

  if (deserializeJson(
        doc,
        server.arg("plain"))) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"invalid json\"}");

    logResponse(400, "invalid json");

    return;
  }

  uint32_t epoch =
    doc["epoch"] | 0;

  if (epoch == 0) {

    server.send(
      400,
      "application/json",
      "{\"error\":\"invalid epoch\"}");

    logResponse(400, "invalid epoch");

    return;
  }

  epoch += (7UL * 3600UL);

  rtc.adjust(DateTime(epoch));

  syncSystemClock();

  SYS.last_sync_millis = millis();

  SYS.rtc_ever_synced = true;

  saveRTCSyncState(true);  // simpan RTC sync state ke nvs

  sysSetRTC(true);  // lanjut write data csv setelah rtc sinkron

  logToFile(
    "🕒 RTC synced! -> CSV write continues!");

  server.send(
    200,
    "application/json",
    "{\"success\":true}");
  logResponse(200);
}

// GET RTC
void handleRTC() {

  DateTime now =
    getNow();

  String json = "{";

  json += "\"timestamp\":\"";
  json += nowStr();
  json += "\",";

  json += "\"epoch\":";
  json += String(
    now.unixtime() - (7UL * 3600UL));
  json += ",";

  json += "\"lost_power\":";
  json += rtc.lostPower()
            ? "true"
            : "false";
  json += ",";

  json += "\"ever_synced\":";
  json += SYS.rtc_ever_synced
            ? "true"
            : "false";
  json += ",";

  json += "\"drift_seconds\":";
  json += SYS.rtc_drift_seconds;

  json += "}";

  server.send(
    200,
    "application/json",
    json);
  logResponse(200);
}

// DEVELOPMENT ONLY | RESET TIME & STATE RTC
void handleRTCClear() {

  resetRTC();  // reset time

  SYS.rtc_ever_synced = false;

  saveRTCSyncState(false);

  sysSetRTC(false);

  logToFile(
    "🧪 RTC time & sync state cleared");

  server.send(
    200,
    "application/json",
    "{\"success\":true}");
  logResponse(200);
}


// ===== LOG REQUEST =====
static void logRequest() {
  String msg = "📡 ";

  switch (server.method()) {
    case HTTP_GET: msg += "GET "; break;
    case HTTP_POST: msg += "POST "; break;
    default: msg += "OTHER "; break;
  }

  msg += server.uri();
  msg += " from ";
  msg += server.client().remoteIP().toString();

  logToFile(msg);
}

// ===== LOG RESPONSE =====
static void logResponse(int code, const char* note) {
  if (note) {
    logToFile("📤 %d %s | %s", code, server.uri().c_str(), note);
  } else {
    logToFile("📤 %d %s", code, server.uri().c_str());
  }
}

// shortcut: register route + auto-log request
#define ROUTE(uri, method, handler) \
  server.on(uri, method, []() { \
    logRequest(); \
    handler(); \
  })

// ===== INIT WEBSERVER =====
void initWebServer() {
  // Collect header cookie
  const char* headerKeys[] = { "Cookie" };
  size_t headerKeysCount = sizeof(headerKeys) / sizeof(char*);
  server.collectHeaders(headerKeys, headerKeysCount);

  // api endpoint
  ROUTE("/", HTTP_GET, []() {
    handleFileRead("/index.html");
  });

  // AUTH
  ROUTE("/api/check", HTTP_GET, handleCheckAuth);
  ROUTE("/api/login", HTTP_GET, handleLogin);
  ROUTE("/api/logout", HTTP_GET, handleLogout);

  // DEVICE NODE
  ROUTE("/api/node/latest", HTTP_GET, handleLatest);    // snapshot hardware & health
  ROUTE("/api/node/estrus", HTTP_GET, handleEstrus);    // informasi model estrus
  ROUTE("/api/node/history", HTTP_GET, handleHistory);  // untuk melihat data csv
  ROUTE("/api/node/health", HTTP_GET, handleHealth);    // untuk cek kesehatan device
  ROUTE("/api/node/device", HTTP_GET, handleDevice);    // identitas device (node_id, mac, firmware)
  ROUTE("/api/download", HTTP_GET, handleDownload);     // untuk download data csv

  // RTC | WAKTU DEVICE
  ROUTE("/api/rtc/sync", HTTP_POST, handleRTCSync);    // untuk sinkronisasi waktu RTC
  ROUTE("/api/rtc", HTTP_GET, handleRTC);              // untuk baca waktu RTC
  ROUTE("/api/rtc/clear", HTTP_POST, handleRTCClear);  // DEVELOPMENT ONLY | RESET TIME & STATE RTC

  // CONFIG
  ROUTE("/api/config", HTTP_GET, handleGetConfig);           // untuk load config dari esp
  ROUTE("/api/config", HTTP_POST, handleSetConfig);          // untuk ubah config
  ROUTE("/api/config/reset", HTTP_POST, handleResetConfig);  // untuk reset config ke default

  // CONTROL ALARM
  ROUTE("/api/alarm/status", HTTP_GET, handleAlarmStatus);  // cek status alarm
  ROUTE("/api/alarm/start", HTTP_POST, handleAlarmStart);   // start alarm
  ROUTE("/api/alarm/stop", HTTP_POST, handleAlarmStop);     // stop alarm

  // SYSTEM
  ROUTE("/api/storage", HTTP_GET, handleStorage);  // untuk cek kondisi SDCard

  ROUTE("/ping", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    logRequest();
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();

  logToFile("✅ WebServer Ready");
}

// diganti jadi task sendiri
void webServerTask(void* pv) {

  while (true) {

    server.handleClient();

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// void handleWebServer() {
//   server.handleClient();
// }
