#define RXD2 16
#define TXD2 17
#define RESPONSE_TIMEOUT_MS 150

const uint8_t readBasicInfoCommand[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
#define READ_BASIC_INFO_COMMAND_SIZE 7

uint8_t step = 1;
uint32_t startTSMS = 0;
uint16_t checksum = 0;
uint16_t frameChecksum = 0;
uint8_t frameDataSize = 0;
uint8_t frameDataRead = 0;
uint16_t hw_totalVoltage10mV = 0;
int16_t hw_current10mAh = 0;
uint16_t hw_remainingCapacity10mAh = 0;
uint16_t hw_nominalCapacity10mAh = 0;
uint16_t hw_errorsBitmask = 0;
uint8_t hw_socPrct = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  if (step > 1 && millis() - startTSMS > RESPONSE_TIMEOUT_MS) {
    Serial.println("Timeout");
    step = 0;
  }
  
  uint8_t c;
  switch (step) {
    case 0:
      delay(3000);
    case 1:
      // Clear out anything still in the buffer.
      while (Serial2.available() > 0) Serial2.read();
      // Send read basic info command frame to BMS.
      Serial2.write(readBasicInfoCommand, READ_BASIC_INFO_COMMAND_SIZE);
      startTSMS = millis();
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
    case 3:
      // Command byte should be the "basic information" command byte.
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      if (c != 0x03) {
        Serial.printf("Wrong command: %02X\n", c);
        step = 0;
        break;
      }
      step = 4;
    case 4:
      // Check status byte.
      if (Serial2.available() == 0) break;
      if (Serial2.read() != 0x00) {
        Serial.printf("Status byte is not correct: %02X\n", c);
        step = 0;
        break;
      }
      // Technically, the status byte is part of the checksum but it will always be 0 at this point.
      //checksum = -c;
      step = 5;
    case 5:
      // Get variable data size.
      if (Serial2.available() == 0) break;
      frameDataSize = Serial2.read();
      checksum = -frameDataSize;
      frameDataRead = 0;
      hw_totalVoltage10mV = 0;
      hw_current10mAh = 0;
      hw_remainingCapacity10mAh = 0;
      hw_nominalCapacity10mAh = 0;
      hw_errorsBitmask = 0;
      hw_socPrct = 0;
      step = 6;
    case 6:
      // Read data.
      while (frameDataRead < frameDataSize && Serial2.available() > 0) {
        c = Serial2.read();
        checksum -= c;
        switch (frameDataRead) {
          case  0: hw_totalVoltage10mV = static_cast<uint16_t>(c) << 8; break;
          case  1: hw_totalVoltage10mV |= static_cast<uint16_t>(c); break;
          case  2: hw_current10mAh = static_cast<int16_t>(c) << 8; break;
          case  3: hw_current10mAh |= static_cast<int16_t>(c); break;
          case  4: hw_remainingCapacity10mAh = static_cast<uint16_t>(c) << 8; break;
          case  5: hw_remainingCapacity10mAh |= static_cast<uint16_t>(c); break;
          case  6: hw_nominalCapacity10mAh = static_cast<uint16_t>(c) << 8; break;
          case  7: hw_nominalCapacity10mAh |= static_cast<uint16_t>(c); break;
          case 16: hw_errorsBitmask = static_cast<uint16_t>(c); break;
          case 19: hw_socPrct = static_cast<uint8_t>(c); break;
        }
        ++frameDataRead;
      }
      if (frameDataRead < frameDataSize) break;
      step = 7;
    case 7:
      // Read checksum (high).
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      frameChecksum = static_cast<uint16_t>(c) << 8;
      step = 8;
    case 8:
      // Read checksum (low) and validate.
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      frameChecksum |= static_cast<uint16_t>(c);
      if (checksum != frameChecksum) {
        Serial.printf("Checksums do not match: %04X != %04X\n", checksum, frameChecksum);
        step = 0;
        break;
      }
      step = 9;
    case 9:
      // Check stop byte.
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      if (c != 0x77) {
        Serial.printf("Stop byte is unexpected: %02X\n", c);
        step = 0;
        break;
      }
      
      if (frameDataRead < 19) {
        Serial.printf("Frame data was too small: %i\n", frameDataRead);
        step = 0;
        break;
      }
      
      Serial.print("======== "); Serial.println(millis());
      Serial.print("Voltage : "); printCents(hw_totalVoltage10mV); Serial.println('V');
      Serial.print("Current : "); printCents(hw_current10mAh); Serial.println('A');
      Serial.print("Capacity: "); printCents(hw_remainingCapacity10mAh); Serial.print('/'); printCents(hw_nominalCapacity10mAh); Serial.println("Ah");
      Serial.print("SoC     : "); Serial.print(hw_socPrct); Serial.println('%');
      Serial.print("Error   : "); Serial.println(hw_errorsBitmask);
      
      step = 0;
  }
}

void printCents(uint16_t val) {
  uint16_t whole = val / 100;
  uint16_t part = val - (whole * 100);
  Serial.print(whole);
  Serial.print('.');
  if (part < 10) Serial.print('0');
  Serial.print(part);
}
