/*
 * ESP32 <-> JBD BMS (JBD-SP21S001) over UART
 * Reads "Basic Information" (command 0x03): pack voltage, current, SOC.
 *
 * Wiring (BMS connector J5, HY2.0-4P, non-isolated):
 *   J5-1 UART-GND  -> ESP32 GND   (REQUIRED common ground)
 *   J5-2 UART-RXD  -> ESP32 GPIO17 (TX2)
 *   J5-3 UART-TXD  -> ESP32 GPIO16 (RX2)
 *   J5-4 UART-B+   -> LEAVE UNCONNECTED (pack positive, ~48-58V, will destroy the ESP32)
 *
 * Serial settings per JBD spec: 9600 baud, 8 data bits, no parity, 1 stop bit.
 */
/*
#define BMS_RX_PIN 16   // ESP32 RX2  <- BMS TXD (J5-3)
#define BMS_TX_PIN 17   // ESP32 TX2  -> BMS RXD (J5-2)

HardwareSerial BMS(2); // use UART2; UART0 stays free for USB/monitor

// Pre-built "read basic info" request: DD A5 03 00 FF FD 77
const uint8_t REQ_BASIC_INFO[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};

uint8_t buf[64];

void setup() {
  Serial.begin(115200);                                 // USB serial monitor
  BMS.begin(9600, SERIAL_8N1, BMS_RX_PIN, BMS_TX_PIN);  // BMS link
  delay(500);
  Serial.println("ESP32 <-> JBD BMS ready");
}

void loop() {
  requestBasicInfo();
  delay(1000);
}

void requestBasicInfo() {
  // Flush any stale bytes, then send the request.
  while (BMS.available()) BMS.read();
  BMS.write(REQ_BASIC_INFO, sizeof(REQ_BASIC_INFO));

  // Wait for a reply (board may be asleep on the first frame; just retry next loop).
  uint32_t t0 = millis();
  while (BMS.available() < 4 && millis() - t0 < 250) delay(2);
  if (BMS.available() < 4) {
    Serial.println("No response (board asleep? retrying next cycle)");
    return;
  }

  // Frame: DD 03 status len [len data bytes] cksumH cksumL 77
  if (BMS.read() != 0xDD) return;   // start byte
  uint8_t cmd    = BMS.read();      // should be 0x03
  uint8_t status = BMS.read();      // 0x00 = OK
  uint8_t len    = BMS.read();      // number of data bytes

  if (cmd != 0x03 || status != 0x00 || len > sizeof(buf)) {
    Serial.printf("Bad frame (cmd=%02X status=%02X len=%u)\n", cmd, status, len);
    return;
  }

  // Read the data field + 2 checksum bytes + stop byte.
  uint32_t want = len + 3;
  t0 = millis();
  while ((uint32_t)BMS.available() < want && millis() - t0 < 250) delay(2);
  if ((uint32_t)BMS.available() < want) {
    Serial.println("Frame timed out mid-read");
    return;
  }
  for (uint8_t i = 0; i < len; i++) buf[i] = BMS.read();
  uint16_t rxSum = (BMS.read() << 8) | BMS.read();  // checksum (big-endian)
  BMS.read();                                       // stop byte 0x77

  // Verify checksum: sum of (status + len + data), inverted, +1.
  uint16_t sum = status + len;
  for (uint8_t i = 0; i < len; i++) sum += buf[i];
  uint16_t calc = (uint16_t)(~sum + 1);
  if (calc != rxSum) {
    Serial.println("Checksum mismatch");
    return;
  }

  parseBasicInfo(len);
}

void parseBasicInfo(uint8_t len) {
  // All 16-bit values are big-endian.
  uint16_t voltRaw = (buf[0] << 8) | buf[1];          // unit 10 mV
  int16_t  currRaw = (int16_t)((buf[2] << 8) | buf[3]);// signed, unit 10 mA (+charge / -discharge)
  uint8_t  soc     = buf[19];                          // RSOC, percent

  float volts = voltRaw / 100.0f;   // 10 mV -> V
  float amps  = currRaw / 100.0f;   // 10 mA -> A

  Serial.printf("Pack: %.2f V   Current: %+.2f A   SOC: %u%%\n", volts, amps, soc);
}
*/
