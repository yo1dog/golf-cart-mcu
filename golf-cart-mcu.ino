#define RXD2 16
#define TXD2 17
#define POLL_INTERVAL_MS 3000
#define RESPONSE_TIMEOUT_MS 150
#define FPS 4

const uint8_t readBasicInfoCommand[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
#define READ_BASIC_INFO_COMMAND_SIZE 7

uint8_t step = 1;
uint32_t stepTSMS = 0;
uint16_t runningChecksum = 0;
uint16_t frameChecksum = 0;
uint8_t frameDataSize = 0;
uint8_t frameDataRead = 0;

struct BMSState {
  uint16_t totalVoltage10mV;
  int16_t current10mAh;
  uint16_t remainingCapacity10mAh;
  uint16_t nominalCapacity10mAh;
  uint16_t protectionBitmask;
  uint8_t socPrct;
};
struct BMSState curBMSState;
struct BMSState latestBMSState;
bool latestBMSStateIsValid = false;

void reset() {
  step = 0;
  stepTSMS = millis();
  runningChecksum = 0;
  frameChecksum = 0;
  frameDataSize = 0;
  frameDataRead = 0;
  curBMSState.totalVoltage10mV = 0;
  curBMSState.current10mAh = 0;
  curBMSState.remainingCapacity10mAh = 0;
  curBMSState.nominalCapacity10mAh = 0;
  curBMSState.protectionBitmask = 0;
  curBMSState.socPrct = 0;
}

bool touch1Toggle = false;
void onTouch1() {touch1Toggle = !touch1Toggle;}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  displaySetup();
}

void loop() {
  bmsLoop();
  displayLoop();
}

void bmsLoop() {
  uint8_t c;
  switch (step) {
    case 0:
      if (millis() - stepTSMS < POLL_INTERVAL_MS) return;
      step = 1;
    case 1:
      // Clear out anything still in the buffer.
      while (Serial2.available() > 0) Serial2.read();
      // Send read basic info command frame to BMS.
      Serial2.write(readBasicInfoCommand, READ_BASIC_INFO_COMMAND_SIZE);
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
    case 3:
      // Command byte should be the "basic information" command byte.
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      if (c != 0x03) {
        Serial.printf("Wrong command: %02X\n", c);
        return reset();
      }
      step = 4;
    case 4:
      // Check status byte.
      if (Serial2.available() == 0) break;
      if (Serial2.read() != 0x00) {
        Serial.printf("Status byte is not correct: %02X\n", c);
        return reset();
      }
      // Technically, the status byte is part of the checksum but it will always be 0 at this point.
      //runningChecksum = -c;
      step = 5;
    case 5:
      // Get variable data size.
      if (Serial2.available() == 0) break;
      frameDataSize = Serial2.read();
      runningChecksum = -frameDataSize;
      step = 6;
    case 6:
      // Read data.
      while (frameDataRead < frameDataSize && Serial2.available() > 0) {
        c = Serial2.read();
        runningChecksum -= c;
        switch (frameDataRead) {
          case  0: curBMSState.totalVoltage10mV = static_cast<uint16_t>(c) << 8; break;
          case  1: curBMSState.totalVoltage10mV |= static_cast<uint16_t>(c); break;
          case  2: curBMSState.current10mAh = static_cast<int16_t>(c) << 8; break;
          case  3: curBMSState.current10mAh |= static_cast<int16_t>(c); break;
          case  4: curBMSState.remainingCapacity10mAh = static_cast<uint16_t>(c) << 8; break;
          case  5: curBMSState.remainingCapacity10mAh |= static_cast<uint16_t>(c); break;
          case  6: curBMSState.nominalCapacity10mAh = static_cast<uint16_t>(c) << 8; break;
          case  7: curBMSState.nominalCapacity10mAh |= static_cast<uint16_t>(c); break;
          case 16: curBMSState.protectionBitmask = static_cast<uint16_t>(c); break;
          case 19: curBMSState.socPrct = static_cast<uint8_t>(c); break;
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
      if (runningChecksum != frameChecksum) {
        Serial.printf("Checksums do not match: %04X != %04X\n", runningChecksum, frameChecksum);
        return reset();
      }
      step = 9;
    case 9:
      // Check stop byte.
      if (Serial2.available() == 0) break;
      c = Serial2.read();
      if (c != 0x77) {
        Serial.printf("Stop byte is unexpected: %02X\n", c);
        return reset();
      }
      
      if (frameDataRead < 19) {
        Serial.printf("Frame data was too small: %i\n", frameDataRead);
        return reset();
      }
      
      latestBMSState = curBMSState;
      latestBMSStateIsValid = true;
      
      /*
      Serial.print("======== "); Serial.println(millis());
      Serial.print("Voltage : "); printCents(curBMSState.totalVoltage10mV); Serial.println('V');
      Serial.print("Current : "); printCents(curBMSState.current10mAh); Serial.println('A');
      Serial.print("Capacity: "); printCents(curBMSState.remainingCapacity10mAh); Serial.print('/'); printCents(curBMSState.nominalCapacity10mAh); Serial.print("Ah ");
        if (curBMSState.nominalCapacity10mAh > 0) printCents((curBMSState.remainingCapacity10mAh * 10000) / curBMSState.nominalCapacity10mAh); else Serial.print("??"); Serial.println("%");
      Serial.print("SoC     : "); Serial.print(curBMSState.socPrct); Serial.println('%');
      Serial.print("Error   : "); Serial.println(curBMSState.protectionBitmask);
      */
      
      return reset();
    // endswitch
  }
  
  if (millis() - stepTSMS >= RESPONSE_TIMEOUT_MS) {
    Serial.println("Timeout");
    return reset();
  }
}


const char8_t* litChar = u8"\u2588";
const char8_t* unlitChar = u8"\u2591";
uint16_t barLEDBitmap = 0;
const uint16_t BAR_LED_0 = 1 << 0;
const uint16_t BAR_LED_1 = 1 << 1;
const uint16_t BAR_LED_2 = 1 << 2;
const uint16_t BAR_LED_3 = 1 << 3;
const uint16_t BAR_LED_4 = 1 << 4;
const uint16_t BAR_LED_5 = 1 << 5;
const uint16_t BAR_LED_6 = 1 << 6;
const uint16_t BAR_LED_7 = 1 << 7;
const uint16_t BAR_LED_8 = 1 << 8;
const uint16_t BAR_LED_9 = 1 << 9;
bool batLight = false;

uint32_t frameTSMS = 0;
void displaySetup() {
  frameTSMS = millis();
}

bool isCharging = true;
uint32_t frame = -1;
const uint32_t displayRefreshIntervalMS = 1000 / FPS;
void displayLoop() {
  uint32_t nowTSMS = millis();
  if (nowTSMS - frameTSMS < displayRefreshIntervalMS) return;
  frameTSMS = nowTSMS;
  ++frame;
  
  if (!latestBMSStateIsValid) {
    if (frame == FPS) {
      Serial.println("No data.");
      frame = 0;
    }
    return;
  }
  
  if (isCharging) {
    uint8_t soc10 = latestBMSState.socPrct / 10;
    if (soc10 > 9) soc10 = 9;
    
    if (frame > soc10) frame = 0;
    barLEDBitmap = (1 << frame) - 1;
  }
  else {
    switch (frame) {
      case FPS: {
        uint8_t soc10 = latestBMSState.socPrct / 10;
        if (soc10 > 9) soc10 = 9;
        barLEDBitmap = 1 << soc10;
        break;
      }
      case FPS*2:
        barLEDBitmap = 0;
        frame = 0;
        break;
      default: return;
    }
  }
  
  Serial.print("======== "); Serial.println(millis());
  Serial.print("Voltage : "); printCents(latestBMSState.totalVoltage10mV); Serial.println('V');
  Serial.print("Current : "); printCents(latestBMSState.current10mAh); Serial.println('A');
  Serial.print("Capacity: "); printCents(latestBMSState.remainingCapacity10mAh); Serial.print('/'); printCents(latestBMSState.nominalCapacity10mAh); Serial.print("Ah ");
    if (latestBMSState.nominalCapacity10mAh > 0) printCents((latestBMSState.remainingCapacity10mAh * 10000) / latestBMSState.nominalCapacity10mAh); else Serial.print("??"); Serial.println("%");
  Serial.print("SoC     : "); Serial.print(latestBMSState.socPrct); Serial.println('%');
  Serial.print("Error   : "); Serial.println(latestBMSState.protectionBitmask);
  Serial.printf(
    "[%s%s%s%s%s%s%s%s%s%s] %s%s%s\n 0123456789  bat\n",
    barLEDBitmap & BAR_LED_0? litChar : unlitChar,
    barLEDBitmap & BAR_LED_1? litChar : unlitChar,
    barLEDBitmap & BAR_LED_2? litChar : unlitChar,
    barLEDBitmap & BAR_LED_3? litChar : unlitChar,
    barLEDBitmap & BAR_LED_4? litChar : unlitChar,
    barLEDBitmap & BAR_LED_5? litChar : unlitChar,
    barLEDBitmap & BAR_LED_6? litChar : unlitChar,
    barLEDBitmap & BAR_LED_7? litChar : unlitChar,
    barLEDBitmap & BAR_LED_8? litChar : unlitChar,
    barLEDBitmap & BAR_LED_9? litChar : unlitChar,
    batLight? litChar : unlitChar,
    batLight? litChar : unlitChar,
    batLight? litChar : unlitChar
  );
}

void printCents(uint16_t val) {
  uint16_t whole = val / 100;
  uint16_t part = val - (whole * 100);
  Serial.print(whole);
  Serial.print('.');
  if (part < 10) Serial.print('0');
  Serial.print(part);
}
