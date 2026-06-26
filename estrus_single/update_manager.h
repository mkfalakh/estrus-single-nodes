#pragma once
#include <Arduino.h>

struct UpdateStatus {

  bool updating = false;
  uint8_t progress = 0;
  String status = "idle";
};

extern UpdateStatus updateStatus;
extern bool otaSuccess;

void handleVersion();
void handleUpdateStatus();
void handleFirmwareUpload();
void handleWebUpload();
void handleUpdateCheck();
