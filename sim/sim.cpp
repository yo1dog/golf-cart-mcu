#include <Arduino.h>
#include <unistd.h>
#include <cstdio>
#include "../config.h"
#include "../bms.h"
#include "../display.h"
#include "bmsSimulator.h"

static constexpr char const*   litChar = "█";
static constexpr char const* unlitChar = "░";

int main() {
  bmsSimulatorSetup();
  bmsSetup();
  displaySetup();
  
  while (true) {
    bmsSimulatorLoop();
    bmsLoop();
    bool didUpdateDisplay = displayLoop();
    
    if (didUpdateDisplay) {
      const BMSState* bms = getBMSState();
      printf("\x1b[0K\n");
      printf("Read Age: %us\x1b[0K\n", (uint32_t)(millis() - bms->readTSMS) / 1000);
      printf("Voltage : %.2fV\x1b[0K\n", bms->totalVoltage10mV / 100.0);
      printf("Current : %.2fA\x1b[0K\n", bms->current10mAh / 100.0);
      printf("Capacity: %.2f/%.0fAh %.2f%%\x1b[0K\n",
        bms->remainingCapacity10mAh / 100.0,
        bms->nominalCapacity10mAh / 100.0,
        bms->nominalCapacity10mAh > 0? (bms->remainingCapacity10mAh * 100.0) / bms->nominalCapacity10mAh : 0.0
      );
      printf("SoC     : %u%%\x1b[0K\n", bms->socPrct);
      printf("Error   : %u\x1b[0K\n", bms->protectionBitmask);
      printf(
        "[%s%s%s%s%s%s%s%s%s%s] %s%s%s\x1b[0K\n 0123456789  bat\x1b[0K\n",
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
      printf("\r\x1b[9A");
    }

    usleep(10000);
    __sim_setMillis(millis() + 10);
  }

  return 0;
}
