#include "bms.h"
#include "config.h"

const uint8_t readBasicInfoCommand[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
const uint8_t readBasicInfoSize = 7;

uint8_t step = 1;
uint32_t stepTSMS = 0;
uint16_t readTSMS = 0;
uint16_t runningChecksum = 0;
uint16_t frameChecksum = 0;
#define FRAME_DATA_MIN_SIZE 23
#define FRAME_DATA_MAX_SIZE 64
uint8_t frameData[FRAME_DATA_MAX_SIZE];
uint8_t frameDataSize = 0;
uint8_t frameDataRead = 0;

BMSState state = {
  isValid = false;
  readTSMS = 0;
  totalVoltage10mV = 0;
  current10mAh = 0;
  remainingCapacity10mAh = 0;
  nominalCapacity10mAh = 0;
  cycleCount = 0;
  productionYear = 0;
  productionMonth = 0;
  productionDay = 0;
  cellBalanceStatusBitmask = 0;
  protectionBitmask = 0;
  softwareVersion = 0;
  socPrct = 0;
  chargeMosfet = false;
  dischargeMosfet = false;
  cellCount = 0;
  tempSensorCount = 0;
  tempSensors10C[0] = 0;
  tempSensors10C[1] = 0;
  tempSensors10C[2] = 0;
  tempSensors10C[3] = 0;
  tempSensors10C[4] = 0;
};

void reset() {
  step = 0;
  stepTSMS = millis();
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
      if (millis() - stepTSMS < BMS_POLL_INTERVAL_MS) return;
      step = 1;
    
    case 1:
      // Clear out anything still in the buffer.
      while (Serial2.available() > 0) Serial2.read();
      
      // Send "read basic information" command frame to BMS.
      Serial2.write(readBasicInfoCommand, readBasicInfoSize);
      stepTSMS = millis();
      step = 2;
    
    case 2:
      // Wait for start byte.
      while (Serial2.available() > 0) {
        if (Serial2.read() == 0xDD) {
          step = 3;
          break;
        }
      }
      if (step != 3) break;
      readTSMS = millis();
    
    case 3:
      // Command byte should be the "basic information" command byte.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      if (b != 0x03) {
        Serial.printf("Wrong command: %02X\n", b);
        return reset();
      }
      step = 4;
    
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
    
    case 6:
      // Read data.
      while (frameDataRead < frameDataSize && Serial2.available() > 0) {
        frameData[frameDataRead++] = Serial2.read();
      }
      if (frameDataRead < frameDataSize) break;
      step = 7;
    
    case 7:
      // Read checksum (high).
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      frameChecksum = (uint16_t)b << 8;
      step = 8;
    
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
    
    case 9:
      // Check stop byte.
      if (Serial2.available() == 0) break;
      b = Serial2.read();
      if (b != 0x77) {
        Serial.printf("Stop byte is unexpected: %02X\n", b);
        return reset();
      }
      
      state.isValue = true;
      state.readTSMS = readTSMS;
      state.totalVoltage10mV = read16(frameData, 0);
      state.current10mAh = (int16_t)read16(frameData, 2);
      state.remainingCapacity10mAh = read16(frameData, 4);
      state.nominalCapacity10mAh = read16(frameData, 6);
      state.cycleCount = read16(frameData, 8);
      uint16_t productionDate = read16(frameData, 10);
      state.productionYear = (production_date >> 9) + 2000;
      state.productionMonth = (uint8_t)((production_date >> 5) & 0x0F);
      state.productionDay = (uint8_t)(production_date & 0x1F);
      state.cellBalanceStatusBitmask = read32(12);
      state.protectionBitmask = read16(frameData, 16);
      state.softwareVersion = frameData[18];
      state.socPrct = frameData[19];
      state.chargeMosfet = frameData[20] & 0x01? true : false;
      state.dischargeMosfet = frameData[20] & 0x02? true : false;
      state.cellCount = frameData[21];
      state.tempSensorCount = frameData[22];
      state.tempSensors10C[0] = state.tempSensorCount > 0? (int16_t)(read16(23) - 2731) : 0;
      state.tempSensors10C[1] = state.tempSensorCount > 1? (int16_t)(read16(25) - 2731) : 0;
      state.tempSensors10C[2] = state.tempSensorCount > 2? (int16_t)(read16(27) - 2731) : 0;
      state.tempSensors10C[3] = state.tempSensorCount > 3? (int16_t)(read16(29) - 2731) : 0;
      state.tempSensors10C[4] = state.tempSensorCount > 4? (int16_t)(read16(31) - 2731) : 0;
      
      return reset();
    // endswitch
  }
  
  if (millis() - stepTSMS >= BMS_RESPONSE_TIMEOUT_MS) {
    Serial.printf("Timeout\n");
    return reset();
  }
}

const BMSState* getBMSState() {
  return &state;
}

uint16_t read16(uint8_t* buff, size_t i) {
  return (
    (uint16_t)buff[i  ] << 8 |
    (uint16_t)buff[i+1]
  );
}
uint32_t read32(uint8_t* buff, size_t i) {
  return  (
    (uint16_t)buff[i  ] << 24 |
    (uint16_t)buff[i+1] << 16 |
    (uint16_t)buff[i+2] <<  8 |
    (uint16_t)buff[i+3]
  );
}
