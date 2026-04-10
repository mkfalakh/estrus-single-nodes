#include "web_server.h"
#include <ESPAsyncWebServer.h>
#include <SD.h>

AsyncWebServer server(80);

void initWebServer() {

  server.on("/api/node/history", HTTP_GET, [](AsyncWebServerRequest *req) {

    if (!req->hasParam("node") || !req->hasParam("date")) {
      req->send(400, "application/json", "{\"error\":\"missing param\"}");
      return;
    }

    String node = req->getParam("node")->value();
    String date = req->getParam("date")->value();
    int page = req->hasParam("page") ? req->getParam("page")->value().toInt() : 0;
    int limit = req->hasParam("limit") ? req->getParam("limit")->value().toInt() : 6;

    String filename = "/log_" + date + ".csv";

    File file = SD.open(filename);
    if (!file) {
      req->send(200, "application/json", "{\"rows\":[],\"has_next\":false}");
      return;
    }

    std::vector<String> all;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.startsWith(node)) {
        all.push_back(line);
      }
    }
    file.close();

    int start = all.size() - (page + 1) * limit;
    int end = all.size() - page * limit;

    if (start < 0) start = 0;
    if (end < 0) end = 0;

    String json = "{\"rows\":[";
    for (int i = end - 1; i >= start; i--) {
      json += "\"" + all[i] + "\"";
      if (i > start) json += ",";
    }

    json += "],\"has_next\":";
    json += (start > 0 ? "true" : "false");
    json += "}";

    req->send(200, "application/json", json);
  });

  server.begin();
}