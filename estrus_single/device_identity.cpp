#include "device_identity.h"
#include <Arduino.h>

/*
generate default Node ID

MAC = xx:xx:xx:9A:8B:2F
↓
NODE-9A8B2F
*/

String generateDefaultNodeId() {

  uint64_t mac = ESP.getEfuseMac();  // ambil mac address esp

  char id[16];

  snprintf(
    id,
    sizeof(id),
    "NODE-%06X",
    (uint32_t)(mac & 0xFFFFFF));

  return String(id);
}


// generate default WIFI AP Password
String generateDefaultAPPassword() {

    return "estrus123";
}
