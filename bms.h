#pragma once

struct BMSState {
  bool isValid;
  uint32_t readTSMS;
  uint16_t totalVoltage10mV;
  int16_t current10mAh;
  uint16_t remainingCapacity10mAh;
  uint16_t nominalCapacity10mAh;
  uint16_t cycleCount;
  uint16_t productionYear;
  uint8_t productionMonth;
  uint8_t productionDay;
  uint32_t cellBalanceStatusBitmask;
  uint16_t protectionBitmask;
  uint8_t softwareVersion;
  uint8_t socPrct;
  bool isChargeMosfetOn;
  bool isDischargeMosfetOn;
  uint8_t cellCount;
  uint8_t tempSensorCount;
  int16_t tempSensors10C[5];
};

void bmsSetup();
void bmsLoop();
const BMSState* getBMSState();
