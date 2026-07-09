#pragma once

/*
Estrus Monitoring v1

WIFI AP Creds: ESTRUS-NODE-xx | estrus123

*/

// Device Version | ganti versi disini jika ada perubahan di firmware atau dashboard. lalu update OTA via dashboard
#define FIRMWARE_VERSION "1.0.0"  // versioning firmware program ESP
#define WEB_VERSION "1.0.0"       // versioning web dashboard

// Pin Proximity
#define PROXI_MODE PROX_ACTIVE_LOW  // atau PROX_ACTIVE_HIGH
#define PROX1_PIN 7                 // JANGAN DIUBAH
#define PROX2_PIN 15                // JANGAN DIUBAH

// Pin Buzzer/Alarm
#define BUZZER_PIN 5          // JANGAN DIUBAH
#define BUZZER_PASSIVE false  // false = tipe buzzer active | sesuaikan jenis buzzer yang dipakai (active/passive)
#define BUZ_FREQ 2000         // untuk buzzer passive
#define BUZ_RES 8             // untuk buzzer passive

// Pin Button Alarm
#define BUZZER_BUTTON_PIN 2  // JANGAN DIUBAH

// Pin Button Restart
#define RESTART_BUTTON_PIN 1  // JANGAN DIUBAH

// I2C
#define I2C_SDA 8  // JANGAN DIUBAH
#define I2C_SCL 9  // JANGAN DIUBAH

// SPI - SD Card
#define SD_SCK 12   // JANGAN DIUBAH
#define SD_MISO 13  // JANGAN DIUBAH
#define SD_MOSI 11  // JANGAN DIUBAH
#define SD_CS 10    // JANGAN DIUBAH

// Pin Battery Divider
#define DIVIDER_PIN 4  // JANGAN DIUBAH

// Pin LED RGB
#define LED_R 16  // JANGAN DIUBAH
#define LED_G 17  // JANGAN DIUBAH
#define LED_B 18  // JANGAN DIUBAH

// INA226
#define INA226_I2C_ADDRESS 0x40  // JANGAN DIUBAH

// Login Creds
#define USER "admin"
#define SALT "SAPI_SALT_2026"                                                           // JANGAN DIUBAH
#define HASHED_PASS "97a813db36482c0678fe50ba536e6093247f6af278a9e77bab6e2a4db2a45eb9"  // password untuk login dashboard | JANGAN DIUBAH

// #define HASHED_PASS "97a813db36482c0678fe50ba536e6093247f6af278a9e77bab6e2a4db2a45eb9" // password untuk login dashboard | JANGAN DIUBAH
