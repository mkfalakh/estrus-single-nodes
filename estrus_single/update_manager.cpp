#include "update_manager.h"
#include "config.h"
#include "http_parser.h"
#include "web_server.h"
#include "logger.h"
#include "config_runtime.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include <Update.h>
#include <WebServer.h>
#include <SD.h>
#include <Arduino.h>
#include <ArduinoJson.h>

UpdateStatus updateStatus;
extern WebServer server;

bool otaSuccess = false;
static size_t totalWritten = 0;
static String incomingFwVersion;
static bool versionChecked = false;

// Helper
bool isNewerVersion(const String& incoming, const String& current) {

  int inMajor = 0, inMinor = 0, inPatch = 0;
  int curMajor = 0, curMinor = 0, curPatch = 0;

  sscanf(
    incoming.c_str(),
    "%d.%d.%d",
    &inMajor,
    &inMinor,
    &inPatch);

  sscanf(
    current.c_str(),
    "%d.%d.%d",
    &curMajor,
    &curMinor,
    &curPatch);

  if (inMajor != curMajor)
    return inMajor > curMajor;

  if (inMinor != curMinor)
    return inMinor > curMinor;

  return inPatch > curPatch;
}

static bool getFirmwareVersionFromBuffer(
  const uint8_t* buf,
  size_t len,
  String& version) {

  size_t needSize =
    sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);

  if (len < needSize) {
    return false;
  }

  const esp_app_desc_t* appDesc =
    (const esp_app_desc_t*)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));

  version =
    String(appDesc->version);

  return true;
}


// GET /api/version | cek versi firmware dan web dashboard
void handleVersion() {

  String json = "{";

  json += "\"firmware_version\":\"";
  json += FIRMWARE_VERSION;
  json += "\",";

  json += "\"web_version\":\"";
  json += WEB_VERSION;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}


// GET /api/update/status | update status ketika upload file
void handleUpdateStatus() {

  String json = "{";

  json += "\"updating\":";
  json += updateStatus.updating ? "true" : "false";

  json += ",\"progress\":";
  json += String(updateStatus.progress);

  json += ",\"status\":\"";
  json += updateStatus.status;
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}


// POST /api/upload/firmware | update firmware ke versi baru
void handleFirmwareUpload() {

  HTTPUpload& upload = server.upload();

  // START
  if (upload.status == UPLOAD_FILE_START) {

    otaSuccess = false;
    totalWritten = 0;
    versionChecked = false;

    updateStatus.updating = true;
    updateStatus.progress = 0;
    updateStatus.status = "uploading";

    logToFile(
      "🚀 OTA Start: %s",
      upload.filename.c_str());

    logToFile(
      "📦 Free OTA Space: %u",
      ESP.getFreeSketchSpace());

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {

      logToFile(
        "❌ OTA begin failed. code=%d",
        Update.getError());

      Update.printError(Serial);

      return;
    }

  }

  // WRITE
  else if (upload.status == UPLOAD_FILE_WRITE) {

    if (!versionChecked) {

      versionChecked = true;

      String binVersion;

      if (getFirmwareVersionFromBuffer(
            upload.buf,
            upload.currentSize,
            binVersion)) {

        logToFile(
          "BIN FW Version: %s",
          binVersion.c_str());

        logToFile(
          "Current FW Version: %s",
          FIRMWARE_VERSION);

        if (!isNewerVersion(
              binVersion,
              FIRMWARE_VERSION)) {

          logToFile(
            "❌ Firmware not newer");

          otaSuccess = false;

          Update.abort();

          return;
        }
      }
    }

    size_t written =
      Update.write(
        upload.buf,
        upload.currentSize);

    if (written != upload.currentSize) {

      logToFile(
        "❌ OTA write failed. code=%d",
        Update.getError());

      otaSuccess = false;

      return;
    }

    totalWritten += written;

    uint8_t pct =
      (upload.totalSize > 0)
        ? (totalWritten * 100ULL / upload.totalSize)
        : 0;

    updateStatus.progress = pct;
  }

  // END
  else if (upload.status == UPLOAD_FILE_END) {

    otaSuccess =
      Update.end(true);

    if (otaSuccess) {

      logToFile(
        "✅ OTA Success");

      updateStatus.progress = 100;
      updateStatus.status = "success";

    } else {

      logToFile(
        "❌ OTA failed. code=%d",
        Update.getError());

      Update.printError(Serial);

      updateStatus.status = "failed";
    }

    updateStatus.updating = false;
  }
}


// POST /api/update/check | cek versi ketika file upload
void handleUpdateCheck() {

  if (!server.hasArg("plain")) {

    server.send(
      400,
      "application/json",
      "{\"success\":false}");

    return;
  }

  JsonDocument doc;

  DeserializationError err =
    deserializeJson(
      doc,
      server.arg("plain"));

  if (err) {

    server.send(
      400,
      "application/json",
      "{\"success\":false}");

    return;
  }

  String incomingFw =
    doc["firmware_version"] | "";

  String incomingWeb =
    doc["web_version"] | "";

  bool firmwareSame =
    (incomingFw == FIRMWARE_VERSION);

  bool firmwareNewer =
    isNewerVersion(
      incomingFw,
      FIRMWARE_VERSION);

  bool webSame =
    (incomingWeb == WEB_VERSION);

  bool webNewer =
    isNewerVersion(
      incomingWeb,
      WEB_VERSION);

  String json = "{";

  json += "\"success\":true,";

  json += "\"firmware_same\":";
  json += firmwareSame ? "true" : "false";

  json += ",\"firmware_newer\":";
  json += firmwareNewer ? "true" : "false";

  json += ",\"web_same\":";
  json += webSame ? "true" : "false";

  json += ",\"web_newer\":";
  json += webNewer ? "true" : "false";

  json += "}";

  server.send(
    200,
    "application/json",
    json);
}


// POST /api/update/web | update web dashboard ke versi baru
// void handleWebUpload() {

//   static File uploadFile;

//   HTTPUpload& upload =
//     server.upload();

//   if (upload.status == UPLOAD_FILE_START) {

//     updateStatus.updating = true;
//     updateStatus.progress = 0;
//     updateStatus.status = "web";

//     SD.mkdir("/update");

//     uploadFile =
//       SD.open(
//         "/update/www.zip",
//         FILE_WRITE);

//     logToFile(
//       "🌐 Web update started");
//   }

//   else if (upload.status == UPLOAD_FILE_WRITE) {

//     if (uploadFile) {

//       uploadFile.write(
//         upload.buf,
//         upload.currentSize);
//     }
//   }

//   else if (upload.status == UPLOAD_FILE_END) {

//     if (uploadFile) {

//       uploadFile.close();
//     }

//     updateStatus.progress = 100;
//     updateStatus.updating = false;
//     updateStatus.status = "uploaded";

//     logToFile(
//       "✅ Web package uploaded");

//     server.send(
//       200,
//       "application/json",
//       "{\"success\":true}");
//   }
// }
