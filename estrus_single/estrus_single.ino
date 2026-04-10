#include "config.h"
#include "sens_ina226.h"
#include "rtc_manager.h"
#include "logger.h"
#include "web_server.h"

QueueHandle_t logQueue;

void sensorTask(void *pv) {
  SensorData data;

  while (true) {

    float voltage = readVoltage();
    float current = readCurrent();

    DateTime now = getNow();

    snprintf(data.timestamp, sizeof(data.timestamp),
             "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    strcpy(data.node_id, NODE_ID);
    data.voltage = voltage;
    data.current = current;

    xQueueSend(logQueue, &data, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
  }
}

void loggerTask(void *pv) {
  SensorData data;

  while (true) {
    if (xQueueReceive(logQueue, &data, portMAX_DELAY)) {
      logToCSV(data);
    }
  }
}

void setup() {
  Serial.begin(115200);

  initINA226();
  initRTC();
  initSDCard();

  logQueue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(SensorData));

  xTaskCreatePinnedToCore(sensorTask, "Sensor", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(loggerTask, "Logger", 4096, NULL, 1, NULL, 1);

  initWebServer();
}

void loop() {}