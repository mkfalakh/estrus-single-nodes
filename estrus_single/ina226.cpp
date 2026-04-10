#include "sensor_ina226.h"
#include "config.h"
#include <Wire.h>
#include <INA226.h>

INA226 ina;

void initINA226() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ina.begin()) {
    Serial.println("❌ INA226 tidak terdeteksi");
    while (1);
  }

  ina.configure(INA226_AVERAGES_16,
                INA226_BUS_CONV_TIME_1100US,
                INA226_SHUNT_CONV_TIME_1100US,
                INA226_MODE_SHUNT_BUS_CONT);

  ina.calibrate(SHUNT_RESISTOR, MAX_CURRENT);

  Serial.println("✅ INA226 siap");
}

float readVoltage() {
  return ina.readBusVoltage();
}

float readCurrent() {
  return ina.readShuntCurrent();
}

float readPower() {
  return ina.readBusPower();
}