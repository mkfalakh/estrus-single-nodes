#include "ina_manager.h"
#include "config.h"
#include "system_state.h"
#include "logger.h"
#include <Wire.h>
#include <INA226_WE.h>

INA226_WE ina226(INA226_I2C_ADDRESS);

// float shuntVoltage_mV = 0.0;
// float loadVoltage_V = 0.0;
// float busVoltage_V = 0.0;
// float current_mA = 0.0;
// float power_mW = 0.0;
// float selisih = 0.0;

// voltCorrection = V multimeter / V ina226 = 0.972
float voltCorrection = 0.972;

bool initINA226() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina226.init()) {
    logToFile("❌ INA init gagal!");
    sysSetINA(false);
    return false;
    // while (1)
    //   ;
  }

  // Optional: averaging biar stabil
  ina226.setAverage(INA226_AVERAGE_64);
  //ina226.setConversionTime(INA226_CONV_TIME_1100); //choose conversion time and uncomment for change of default
  // ina226.setMeasureMode(INA226_CONTINUOUS);  // choose mode and uncomment for change of default

  // 🔥 KALIBRASI
  ina226.setResistorRange(0.1, 0.9);  // Rshunt=0.1Ω, Imax= 0.1/100mA|0.01/10mA
  // ina226.setCorrectionFactor(0.972);

  logToFile("✅ INA OK");
  sysSetINA(true);
  return true;
}

float readVoltage() {
  // bus voltage (V)
  return ina226.getBusVoltage_V() * voltCorrection;
}

float readCurrent() {
  // shunt current (mA)
  return ina226.getCurrent_mA();
}

float readPower() {
  // bus power (mW)
  return ina226.getBusPower();
}


// check INA health
bool checkINAHealth() {

  Wire.beginTransmission(INA226_I2C_ADDRESS);

  uint8_t err = Wire.endTransmission();

  if (err != 0) {

    logToFile("⚠️ INA Wire error!");

    sysSetINA(false);

    return false;
  }

  sysSetINA(true);

  return true;
}
