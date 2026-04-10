#include "logger.h"
#include <SD.h>

void initSDCard() {
  if (!SD.begin()) {
    Serial.println("❌ SD gagal");
    while (1);
  }
}

String getFilenameFromDate(const char *timestamp) {
  // format: YYYY-MM-DD HH:MM:SS
  String date = String(timestamp).substring(0, 10);
  return "/log_" + date + ".csv";
}

void logToCSV(const SensorData &data) {
  String filename = getFilenameFromDate(data.timestamp);

  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    Serial.println("❌ gagal buka file");
    return;
  }

  file.printf("%s,%s,%.2f,%.2f\n",
              data.node_id,
              data.timestamp,
              data.voltage,
              data.current);

  file.close();
}