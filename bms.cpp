#include <Arduino.h>
#include "config.h"
#include "bms.h"

static constexpr uint32_t POLL_INTERVAL_MS = BMS_POLL_INTERVAL_MS;
static constexpr uint32_t RESPONSE_TIMEOUT_MS = BMS_RESPONSE_TIMEOUT_MS;

static constexpr uint8_t readBasicInfoCommand[] = {0xDD,0xA5,0x03,0x00,0xFF,0xFD,0x77};
static constexpr uint8_t readBasicInfoCommandSize = 7;

static uint16_t read16(uint8_t* buff, size_t i);
static uint32_t read32(uint8_t* buff, size_t i);

static uint8_t step = 1;
static uint32_t lastPollTSMS = -POLL_INTERVAL_MS;
static uint32_t readTSMS = 0;
static uint16_t runningChecksum = 0;
static uint16_t frameChecksum = 0;

static constexpr uint8_t FRAME_DATA_MIN_SIZE = 23;
static constexpr uint8_t FRAME_DATA_MAX_SIZE = 64;
static uint8_t frameData[FRAME_DATA_MAX_SIZE];
static uint8_t frameDataSize = 0;
static uint8_t frameDataRead = 0;

static BMSState state = {};

static void reset() {
  step = 0;
  readTSMS = 0;
  runningChecksum = 0;
  frameChecksum = 0;
  frameDataSize = 0;
  frameDataRead = 0;
}

void bmsSetup() {
  Serial2.begin(9600, SERIAL_8N1, BMS_RXD_PIN, BMS_TXD_PIN);
  reset();
}

void bmsLoop() {
  uint8_t b;
  switch (step) {
    case 0:
      // Wait for poll interval.
      if ((uint32_t)(millis() - lastPollTSMS) < POLL_INTERVAL_MS) return;
      step = 1;
      [[fallthrough]];

    case 1:
      // Clear out anything still in the buffer.
      while (Serial2.available() > 0) Serial2.read();
      
      // Send "read basic information" command frame to BMS.
      Serial2.write(readBasicInfoCommand, readBasicInfoCommandSize);
      lastPollTSMS = millis();
      step = 2;
      [[fallthrough]];

    case 2:
      // Wait for start byte.
      while (Serial2.available() > 0) {
        if (Serial2.read() == 0xDD) {
          step = 3;
          readTSMS = millis();
          break;
        }
      }
      if (step != 3) break;
      [[fallthrough]];

    case 3:
      // Command byte should be the "basic information" command byte.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      if (b != 0x03) {
        Serial.printf("Wrong command: %02X\n", b);
        return reset();
      }
      step = 4;
      [[fallthrough]];

    case 4:
      // Check status byte.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      if (b != 0x00) {
        Serial.printf("Status byte is not correct: %02X\n", b);
        return reset();
      }
      runningChecksum = -b;
      step = 5;
      [[fallthrough]];

    case 5:
      // Get variable data size.
      if (Serial2.available() == 0) break;
      frameDataSize = Serial2.read();
      
      if (frameDataSize > FRAME_DATA_MAX_SIZE) {
        Serial.printf("Frame data is too big: %u\n", frameDataSize);
        return reset();
      }
      if (frameDataSize < FRAME_DATA_MIN_SIZE) {
        Serial.printf("Frame data is too small: %u\n", frameDataSize);
        return reset();
      }
      
      runningChecksum -= frameDataSize;
      step = 6;
      [[fallthrough]];

    case 6:
      // Read data.
      while (frameDataRead < frameDataSize && Serial2.available() > 0) {
        b = Serial2.read();
        frameData[frameDataRead++] = b;
        runningChecksum -= b;
      }
      if (frameDataRead < frameDataSize) break;
      step = 7;
      [[fallthrough]];

    case 7:
      // Read checksum (high).
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      frameChecksum = (uint16_t)b << 8;
      step = 8;
      [[fallthrough]];

    case 8:
      // Read checksum (low) and validate.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      frameChecksum |= (uint16_t)b;
      if (runningChecksum != frameChecksum) {
        Serial.printf("Checksums do not match: %04X != %04X\n", runningChecksum, frameChecksum);
        return reset();
      }
      step = 9;
      [[fallthrough]];

    case 9: {
      // Check stop byte.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      if (b != 0x77) {
        Serial.printf("Stop byte is unexpected: %02X\n", b);
        return reset();
      }
      
      state.isValid = true;
      state.readTSMS = readTSMS;
      state.totalVoltage10mV = read16(frameData, 0);
      state.current10mAh = (int16_t)read16(frameData, 2);
      state.remainingCapacity10mAh = read16(frameData, 4);
      state.nominalCapacity10mAh = read16(frameData, 6);
      state.cycleCount = read16(frameData, 8);
      uint16_t productionDate = read16(frameData, 10);
      state.productionYear = (productionDate >> 9) + 2000;
      state.productionMonth = (uint8_t)((productionDate >> 5) & 0x0F);
      state.productionDay = (uint8_t)(productionDate & 0x1F);
      state.cellBalanceStatusBitmask = read32(frameData, 12);
      state.protectionBitmask = read16(frameData, 16);
      state.softwareVersion = frameData[18];
      state.socPrct = frameData[19];
      state.isChargeMosfetOn = frameData[20] & 0x01? true : false;
      state.isDischargeMosfetOn = frameData[20] & 0x02? true : false;
      state.cellCount = frameData[21];
      state.tempSensorCount = frameData[22];
      state.tempSensors10C[0] = state.tempSensorCount > 0? (int16_t)(read16(frameData, 23) - 2731) : 0;
      state.tempSensors10C[1] = state.tempSensorCount > 1? (int16_t)(read16(frameData, 25) - 2731) : 0;
      state.tempSensors10C[2] = state.tempSensorCount > 2? (int16_t)(read16(frameData, 27) - 2731) : 0;
      state.tempSensors10C[3] = state.tempSensorCount > 3? (int16_t)(read16(frameData, 29) - 2731) : 0;
      state.tempSensors10C[4] = state.tempSensorCount > 4? (int16_t)(read16(frameData, 31) - 2731) : 0;
      
      return reset();
    }
    // endswitch
  }
  
  if ((uint32_t)(millis() - lastPollTSMS) >= RESPONSE_TIMEOUT_MS) {
    Serial.printf("Timeout\n");
    return reset();
  }
}

const BMSState* getBMSState() {
  return &state;
}

static uint16_t read16(uint8_t* buff, size_t i) {
  return (
    (uint16_t)buff[i  ] << 8 |
    (uint16_t)buff[i+1]
  );
}
static uint32_t read32(uint8_t* buff, size_t i) {
  return  (
    (uint32_t)buff[i  ] << 24 |
    (uint32_t)buff[i+1] << 16 |
    (uint32_t)buff[i+2] <<  8 |
    (uint32_t)buff[i+3]
  );
}
