#include "bms.h"
#include "display.h"

#define DEBUG 0
#if DEBUG
#include "config.h"
static constexpr char const*   litChar = "█";
static constexpr char const* unlitChar = "░";

static constexpr uint32_t PRINT_INTERVAL_MS = 500;
static uint32_t lastPrintTSMS = -PRINT_INTERVAL_MS;
#endif

void setup() {
  Serial.begin(115200);
  bmsSetup();
  displaySetup();
}

void loop() {
  bmsLoop();
  displayLoop();
  
  #if DEBUG
  if ((uint32_t)(millis() - lastPrintTSMS) >= PRINT_INTERVAL_MS) {
    lastPrintTSMS = millis();
    const BMSState* bms = getBMSState();
    printf("=========\n");
    printf("Read Age: %us\n", (uint32_t)(millis() - bms->readTSMS) / 1000);
    printf("Voltage : %.2fV\n", bms->totalVoltage10mV / 100.0);
    printf("Current : %.2fA\n", bms->current10mAh / 100.0);
    printf("Capacity: %.2f/%.0fAh %.2f%%\n",
      bms->remainingCapacity10mAh / 100.0,
      bms->nominalCapacity10mAh / 100.0,
      bms->nominalCapacity10mAh > 0? (bms->remainingCapacity10mAh * 100.0) / bms->nominalCapacity10mAh : 0.0
    );
    printf("SoC     : %u%%\n", bms->socPrct);
    printf("Error   : %u\n", bms->protectionBitmask);
    
    printf(
      "[%s%s%s%s%s%s%s%s%s%s] %s%s%s\n 0123456789  bat\n",
      digitalRead(BAR_LED_0_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_1_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_2_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_3_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_4_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_5_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_6_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_7_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_8_PIN)? litChar : unlitChar,
      digitalRead(BAR_LED_9_PIN)? litChar : unlitChar,
      digitalRead(BATTERY_LIGHT_PIN)? litChar : unlitChar,
      digitalRead(BATTERY_LIGHT_PIN)? litChar : unlitChar,
      digitalRead(BATTERY_LIGHT_PIN)? litChar : unlitChar
    );
  }
  #endif
}
