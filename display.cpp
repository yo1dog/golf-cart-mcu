#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "bms.h"

static constexpr uint8_t FPS = DISPLAY_FPS;
static constexpr uint32_t BMS_STALE_AGE_MS = DISPLAY_BMS_STALE_AGE_MS;
static constexpr uint32_t REFRESH_INTERVAL_MS = 1000 / FPS;

static constexpr uint16_t BAR_LED_0 = 1 << 0;
static constexpr uint16_t BAR_LED_1 = 1 << 1;
static constexpr uint16_t BAR_LED_2 = 1 << 2;
static constexpr uint16_t BAR_LED_3 = 1 << 3;
static constexpr uint16_t BAR_LED_4 = 1 << 4;
static constexpr uint16_t BAR_LED_5 = 1 << 5;
static constexpr uint16_t BAR_LED_6 = 1 << 6;
static constexpr uint16_t BAR_LED_7 = 1 << 7;
static constexpr uint16_t BAR_LED_8 = 1 << 8;
static constexpr uint16_t BAR_LED_9 = 1 << 9;

static void updateBarDisplay(uint16_t barLEDBitmap);
static void updateBatteryLight(bool enable);

static constexpr uint8_t ANIM_LOADING  = 0;
static constexpr uint8_t ANIM_ERROR    = 1;
static constexpr uint8_t ANIM_SOC      = 2;
static constexpr uint8_t ANIM_CHARGING = 3;

static uint8_t lastAnimation = ANIM_LOADING;
static uint32_t frame = -1;
static uint32_t lastFrameTSMS = -REFRESH_INTERVAL_MS;

static uint8_t binSOC10(uint8_t socPrct) {
  // Bin the SoC into 10 segments: 0-9 rounded down.
  uint8_t soc10 = socPrct / 10;
  if (soc10 > 9) soc10 = 9;
  return soc10;
}

void displaySetup() {
  pinMode(BAR_LED_0_PIN, OUTPUT); digitalWrite(BAR_LED_0_PIN, LOW);
  pinMode(BAR_LED_1_PIN, OUTPUT); digitalWrite(BAR_LED_1_PIN, LOW);
  pinMode(BAR_LED_2_PIN, OUTPUT); digitalWrite(BAR_LED_2_PIN, LOW);
  pinMode(BAR_LED_3_PIN, OUTPUT); digitalWrite(BAR_LED_3_PIN, LOW);
  pinMode(BAR_LED_4_PIN, OUTPUT); digitalWrite(BAR_LED_4_PIN, LOW);
  pinMode(BAR_LED_5_PIN, OUTPUT); digitalWrite(BAR_LED_5_PIN, LOW);
  pinMode(BAR_LED_6_PIN, OUTPUT); digitalWrite(BAR_LED_6_PIN, LOW);
  pinMode(BAR_LED_7_PIN, OUTPUT); digitalWrite(BAR_LED_7_PIN, LOW);
  pinMode(BAR_LED_8_PIN, OUTPUT); digitalWrite(BAR_LED_8_PIN, LOW);
  pinMode(BAR_LED_9_PIN, OUTPUT); digitalWrite(BAR_LED_9_PIN, LOW);
  pinMode(BATTERY_LIGHT_PIN, OUTPUT); digitalWrite(BATTERY_LIGHT_PIN, LOW);
}

bool displayLoop() {
  // Advance frame at designated FPS rate.
  uint32_t nowTSMS = millis();
  if ((uint32_t)(nowTSMS - lastFrameTSMS) < REFRESH_INTERVAL_MS) return false;
  lastFrameTSMS = nowTSMS;
  ++frame;
  
  const BMSState* bms = getBMSState();
  
  // Evaluate which animation we should be displaying.
  uint8_t animation = (
    // If BMS state is not valid or is stale, show loading.
    (
      !bms->isValid ||
      (uint32_t)(nowTSMS - bms->readTSMS) > BMS_STALE_AGE_MS // This stale check can wrap.
    )? ANIM_LOADING :
    
    // If BMS reports an active protection, show error.
    bms->protectionBitmask != 0? ANIM_ERROR :
    
    // If the current is positive, the battery is being charged. Show charging.
    bms->current10mAh > 0? ANIM_CHARGING :
    
    // Otherwise, show the SoC.
    ANIM_SOC
  );
  
  if (animation != lastAnimation) {
    // On animation change, reset the frame index.
    frame = 0;
    lastAnimation = animation;
  }
  
  if (animation == ANIM_SOC) {
    // Show battery light if SoC is under 20%.
    updateBatteryLight(bms->socPrct < 20);
    
    // Show SoC.
    frame = 0;
    uint8_t soc10 = binSOC10(bms->socPrct);
    updateBarDisplay(1 << soc10);
  }
  else if (animation == ANIM_CHARGING) {
    // Don't show battery light.
    updateBatteryLight(false);
    
    // Fill the bar from left-to-right to the current SoC (starting with blank), hold for 2 seconds, then reset.
    // Remember that SoC can change mid-animation.
    uint8_t soc10 = binSOC10(bms->socPrct);
    if (frame > (uint32_t)(soc10 + 1 + FPS*2)) frame = 0;
    if (frame <= (uint32_t)(soc10 + 1)) {
      // (1 << x) - 1 results in the first x bits being 1. EX: x=6 -> (1 << 6) - 1 -> 01000000 - 1 -> 00111111
      updateBarDisplay((1 << frame) - 1);
    }
    else {
      updateBarDisplay((1 << (soc10 + 1)) - 1);
    }
  }
  else if (animation == ANIM_ERROR) {
    // Blink the battery light.
    bool blink = frame % 10 < 5;
    updateBatteryLight(blink);
    
    if (frame >= FPS*15) frame = 0;
    if (frame < FPS*5) {
      // For the first 5 seconds, display the error low byte on LEDs 0-7 + blink LED 8.
      uint8_t bitmaskLowByte = bms->protectionBitmask & 0x00FF;
      updateBarDisplay((blink? BAR_LED_8 : 0) | bitmaskLowByte);
    }
    else if (frame < FPS*10) {
      // For the next 5 seconds, display the error high byte on LEDs 0-7 + blink LED 9.
      uint8_t bitmaskHighByte = (bms->protectionBitmask & 0xFF00) >> 8;
      updateBarDisplay((blink? BAR_LED_9 : 0) | bitmaskHighByte);
    }
    else {
      // For the next 5 seconds, show SoC.
      uint8_t soc10 = binSOC10(bms->socPrct);
      updateBarDisplay(1 << soc10);
    }
  }
  else { // Assume animation == ANIM_LOADING
    // Disable the battery light.
    updateBatteryLight(false);
    
    // Always light the 2 border LEDs and bounce a light along the 8 LEDs between the borders.
    // 1 + {0,1,2,3,4,5,6,7,6,5,4,3,2,1}
    if (frame >= 14) frame = 0;
    uint8_t curLED = 1 + (
      frame < 8
      ? frame      // 0-> 7 = 0->7
      : 14 - frame // 8->13 = 6->1
    );
    updateBarDisplay(BAR_LED_9 | (1 << curLED) | BAR_LED_0);
  }
  
  return true;
}

static uint16_t _curBarLEDBitmap = 0;
static void updateBarDisplay(uint16_t barLEDBitmap) {
  // Only write to pins when their values change. TODO: Unnecessary performance optimization?
  if ((barLEDBitmap & BAR_LED_0) != (_curBarLEDBitmap & BAR_LED_0)) digitalWrite(BAR_LED_0_PIN, barLEDBitmap & BAR_LED_0? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_1) != (_curBarLEDBitmap & BAR_LED_1)) digitalWrite(BAR_LED_1_PIN, barLEDBitmap & BAR_LED_1? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_2) != (_curBarLEDBitmap & BAR_LED_2)) digitalWrite(BAR_LED_2_PIN, barLEDBitmap & BAR_LED_2? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_3) != (_curBarLEDBitmap & BAR_LED_3)) digitalWrite(BAR_LED_3_PIN, barLEDBitmap & BAR_LED_3? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_4) != (_curBarLEDBitmap & BAR_LED_4)) digitalWrite(BAR_LED_4_PIN, barLEDBitmap & BAR_LED_4? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_5) != (_curBarLEDBitmap & BAR_LED_5)) digitalWrite(BAR_LED_5_PIN, barLEDBitmap & BAR_LED_5? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_6) != (_curBarLEDBitmap & BAR_LED_6)) digitalWrite(BAR_LED_6_PIN, barLEDBitmap & BAR_LED_6? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_7) != (_curBarLEDBitmap & BAR_LED_7)) digitalWrite(BAR_LED_7_PIN, barLEDBitmap & BAR_LED_7? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_8) != (_curBarLEDBitmap & BAR_LED_8)) digitalWrite(BAR_LED_8_PIN, barLEDBitmap & BAR_LED_8? HIGH : LOW);
  if ((barLEDBitmap & BAR_LED_9) != (_curBarLEDBitmap & BAR_LED_9)) digitalWrite(BAR_LED_9_PIN, barLEDBitmap & BAR_LED_9? HIGH : LOW);
  _curBarLEDBitmap = barLEDBitmap;
}

static bool _curBatteryLight = false;
static void updateBatteryLight(bool enable) {
  if (_curBatteryLight == enable) return;
  digitalWrite(BATTERY_LIGHT_PIN, enable? HIGH : LOW);
  _curBatteryLight = enable;
}
