#include <Arduino.h>
#include <cstdio>
#include "bmsSimulator.h"
#include "../bms.h"

static constexpr uint8_t readBasicInfoCommand[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
static constexpr uint8_t readBasicInfoCommandSize = 7;

static constexpr uint32_t RESPONSE_DELAY_MS = 15;
static constexpr uint32_t MIN_FILE_READ_INTERVAL_MS = 100;

static void refreshStateFromFile();
static void sendResponseFrame();
static void write8 (uint8_t* buff, uint8_t i, uint8_t  val);
static void write16(uint8_t* buff, uint8_t i, uint16_t val);
static void write32(uint8_t* buff, uint8_t i, uint32_t val);

static FILE* bmsFile = nullptr;
static BMSState state = {};
static uint8_t requestFrameMatchedLen = 0;
static uint8_t pendingResponseCount = 0;
static uint32_t responseAnchorTSMS = -RESPONSE_DELAY_MS;
static uint32_t lastFileReadTSMS = -MIN_FILE_READ_INTERVAL_MS;

void bmsSimulatorSetup() {
  bmsFile = fopen("sim/bms.txt", "r");
  if (bmsFile == nullptr) throw "Failed to open sim/bms.txt";
}

void bmsSimulatorLoop() {
  while (Serial2.__sim_txAvailable() > 0) {
    uint8_t b = Serial2.__sim_txRead();
    if (b != readBasicInfoCommand[requestFrameMatchedLen]) {
      requestFrameMatchedLen = 0;
      continue;
    }
    
    ++requestFrameMatchedLen;
    if (requestFrameMatchedLen == readBasicInfoCommandSize) {
      if (pendingResponseCount == 0) {
        responseAnchorTSMS = millis();
      }
      ++pendingResponseCount;
      requestFrameMatchedLen = 0;
    }
  }
  
  if (
    pendingResponseCount > 0 &&
    (uint32_t)(millis() - responseAnchorTSMS) >= RESPONSE_DELAY_MS
  ) {
    refreshStateFromFile();
    if (state.isValid) {
      sendResponseFrame();
    }
    --pendingResponseCount;
    responseAnchorTSMS = millis();
  }
}

static void refreshStateFromFile() {
  if ((uint32_t)(millis() - lastFileReadTSMS) < MIN_FILE_READ_INTERVAL_MS) {
    return;
  }
  lastFileReadTSMS = millis();

  fflush(bmsFile);
  rewind(bmsFile);
  
  uint8_t isValidInt = 0;
  uint8_t isChargeMosfetOnInt = 0;
  uint8_t isDischargeMosfetOnInt = 0;
  int result = fscanf(
    bmsFile,
    "isValid = %hhu\n"
    "totalVoltage10mV = %hu\n"
    "current10mAh = %hd\n"
    "remainingCapacity10mAh = %hu\n"
    "nominalCapacity10mAh = %hu\n"
    "cycleCount = %hu\n"
    "productionYear = %hu\n"
    "productionMonth = %hhu\n"
    "productionDay = %hhu\n"
    "cellBalanceStatusBitmask = %u\n"
    "protectionBitmask = %hu\n"
    "softwareVersion = %hhu\n"
    "socPrct = %hhu\n"
    "isChargeMosfetOn = %hhu\n"
    "isDischargeMosfetOn = %hhu\n"
    "cellCount = %hhu\n"
    "tempSensorCount = %hhu\n"
    "tempSensors10C[0] = %hd\n"
    "tempSensors10C[1] = %hd\n"
    "tempSensors10C[2] = %hd\n"
    "tempSensors10C[3] = %hd\n"
    "tempSensors10C[4] = %hd",
    &isValidInt,
    &state.totalVoltage10mV,
    &state.current10mAh,
    &state.remainingCapacity10mAh,
    &state.nominalCapacity10mAh,
    &state.cycleCount,
    &state.productionYear,
    &state.productionMonth,
    &state.productionDay,
    &state.cellBalanceStatusBitmask,
    &state.protectionBitmask,
    &state.softwareVersion,
    &state.socPrct,
    &isChargeMosfetOnInt,
    &isDischargeMosfetOnInt,
    &state.cellCount,
    &state.tempSensorCount,
    &state.tempSensors10C[0],
    &state.tempSensors10C[1],
    &state.tempSensors10C[2],
    &state.tempSensors10C[3],
    &state.tempSensors10C[4]
  );
  state.isValid = isValidInt? true : false;
  state.isChargeMosfetOn = isChargeMosfetOnInt? true : false;
  state.isDischargeMosfetOn = isDischargeMosfetOnInt? true : false;
  if (result != 22) {
    state.isValid = false;
    printf("Contents of file sim/bms.txt is not valid.\n");
  }
}

static void sendResponseFrame() {
  uint8_t frame[40];
  uint16_t productionDate = (
    (uint16_t)(state.productionYear - 2000) << 9 |
    (uint16_t)state.productionMonth << 5 |
    (uint16_t)state.productionDay
  );
  uint8_t mosfetBitmask = (
    (state.isChargeMosfetOn   ? 0x01 : 0) |
    (state.isDischargeMosfetOn? 0x02 : 0)
  );
  
  write8 (frame, 0, 0xDD); // start
  write8 (frame, 1, 0x03); // command
  write8 (frame, 2, 0x00); // status
  write8 (frame, 3, 33  ); // data size
  write16(frame, 4, state.totalVoltage10mV);
  write16(frame, 6, (uint16_t)state.current10mAh);
  write16(frame, 8, state.remainingCapacity10mAh);
  write16(frame, 10, state.nominalCapacity10mAh);
  write16(frame, 12, state.cycleCount);
  write16(frame, 14, productionDate);
  write32(frame, 16, state.cellBalanceStatusBitmask);
  write16(frame, 20, state.protectionBitmask);
  write8 (frame, 22, state.softwareVersion);
  write8 (frame, 23, state.socPrct);
  write8 (frame, 24, mosfetBitmask);
  write8 (frame, 25, state.cellCount);
  write8 (frame, 26, state.tempSensorCount);
  write16(frame, 27, (uint16_t)(state.tempSensors10C[0] + 2731));
  write16(frame, 29, (uint16_t)(state.tempSensors10C[1] + 2731));
  write16(frame, 31, (uint16_t)(state.tempSensors10C[2] + 2731));
  write16(frame, 33, (uint16_t)(state.tempSensors10C[3] + 2731));
  write16(frame, 35, (uint16_t)(state.tempSensors10C[4] + 2731));
  
  uint16_t checksum = 0;
  for (int i = 2; i < 37; ++i) checksum -= frame[i];
  
  write16(frame, 37, checksum);
  write8 (frame, 39, 0x77); // stop
  
  Serial2.__sim_rxWrite(frame, 40);
}

static void write8(uint8_t* buff, uint8_t i, uint8_t val) {
  buff[i] = val;
}
static void write16(uint8_t* buff, uint8_t i, uint16_t val) {
  buff[i  ] = (uint8_t)(val >> 8);
  buff[i+1] = (uint8_t)(val);
}
static void write32(uint8_t* buff, uint8_t i, uint32_t val) {
  buff[i  ] = (uint8_t)(val >> 24);
  buff[i+1] = (uint8_t)(val >> 16);
  buff[i+2] = (uint8_t)(val >> 8);
  buff[i+3] = (uint8_t)(val);
}