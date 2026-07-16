#include "wifi_task.h"
#include "wifi_manager.h"
#include "logger.h"
#include "system_state.h"

#define WIFI_TIMEOUT_MS 600000UL  // 10 menit

// void wifiTask(void *pv) {

//   while (true) {

//     if (wifiEnabled) {

//       unsigned long idleTime =
//         millis() - lastClientTime;

//       if (idleTime >= WIFI_TIMEOUT_MS) {

//         logToFile(
//           "WiFi idle=%lu timeout=%lu enabled=%d",
//           idleTime,
//           WIFI_TIMEOUT_MS,
//           wifiEnabled);

//         disableWiFiAP();
//       }
//     }

//     vTaskDelay(
//       5000 / portTICK_PERIOD_MS);
//   }
// }
