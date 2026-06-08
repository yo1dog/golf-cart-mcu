#include <cstdint>
#include <unistd.h>
#include <cstdio>

#define FPS 4

void printCents(uint16_t val);
uint32_t millis();

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

const char* litChar = "\u2588";
const char* unlitChar = "\u2591";
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


#define ANIM_LEVEL 0
#define ANIM_CHARGE 1
#define ANIM_ERROR 2
#define ANIM_LOADING 3

uint8_t curAnimation = ANIM_LEVEL;
uint32_t frame = -1;
const uint32_t displayRefreshIntervalMS = 1000 / FPS;
void displayLoop() {
  uint32_t nowTSMS = millis();
  if (nowTSMS - frameTSMS < displayRefreshIntervalMS) return;
  frameTSMS = nowTSMS;
  ++frame;
  
  uint8_t animation = (
    !latestBMSStateIsValid? ANIM_LOADING :
    latestBMSState.protectionBitmask != 0? ANIM_ERROR :
    latestBMSState.current10mAh < 0? ANIM_CHARGE :
    ANIM_LEVEL
  );
  if (animation != curAnimation) {
    frame = 0;
    curAnimation = animation;
  }
  
  uint8_t soc10 = latestBMSState.socPrct / 10;
  if (soc10 > 9) soc10 = 9;
  
  switch (animation) {
    case ANIM_LEVEL: {
      batLight = latestBMSState.socPrct < 20;
      
      switch (frame) {
        case FPS*2:
          frame = 0;
        case 0: {
          barLEDBitmap = 1 << soc10;
          break;
        }
        case FPS:
          barLEDBitmap = 0;
          break;
        default: return;
      }
      break;
    }
    
    case ANIM_CHARGE: {
      batLight = false;
      
      if (frame > soc10 + 1) frame = 0;
      barLEDBitmap = (1 << frame) - 1;
      break;
    }
    
    case ANIM_ERROR: {
      batLight = !batLight;
      
      switch (frame) {
        case FPS*6:
          frame = 0;
        case 0:
          barLEDBitmap = 1 << soc10;
          break;
        case FPS*2:
          barLEDBitmap = BAR_LED_9 | BAR_LED_8 | (latestBMSState.protectionBitmask & 0x00FF);
          break;
        case FPS*4:
          barLEDBitmap = BAR_LED_9 | BAR_LED_8 | ((latestBMSState.protectionBitmask & 0xFF00) >> 8);
          break;
      }
      
      break;
    }
    
    case ANIM_LOADING: {
      if (frame > 13) frame = 0;
      barLEDBitmap = BAR_LED_9 | (1 << (1 + (frame > 7? 14 - frame : frame))) | BAR_LED_0;
    }
  }
  
  // printf("======== %u\n", millis());
  printf("Voltage : "); printCents(latestBMSState.totalVoltage10mV); printf("V\x1b[0K\n");
  printf("Current : "); printCents(latestBMSState.current10mAh); printf("A\x1b[0K\n");
  printf("Capacity: "); printCents(latestBMSState.remainingCapacity10mAh); printf("/"); printCents(latestBMSState.nominalCapacity10mAh); printf("Ah ");
    if (latestBMSState.nominalCapacity10mAh > 0) printCents((latestBMSState.remainingCapacity10mAh * 10000) / latestBMSState.nominalCapacity10mAh); else printf("??"); printf("%%\x1b[0K\n");
  printf("SoC     : %u%%\x1b[0K\n", latestBMSState.socPrct);
  printf("Error   : %u\x1b[0K\n", latestBMSState.protectionBitmask);
  printf("Anim    : %u %u\x1b[0K\n", animation, frame);
  printf(
    "[%s%s%s%s%s%s%s%s%s%s] %s%s%s\x1b[0K\n 0123456789  bat\x1b[0K\n",
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
  printf("\r\x1b[8A");
}


void printCents(uint16_t val) {
  uint16_t whole = val / 100;
  uint16_t part = val - (whole * 100);
  printf("%u.%s%u", whole, part < 10? "0" : "", part);
}

uint32_t _millis = 0;
uint32_t millis() {
  return _millis;
}
int main() {
  displaySetup();
  
  while (true) {
    if (_millis % 100 == 0) {
      FILE * file = fopen("state.txt", "r");
      if (file == NULL) {
        fputs("Error opening state.txt\n", stderr);
        return 1;
      }
      
      int result = fscanf(
        file,
        "totalVoltage10mV=%hu\ncurrent10mAh=%hd\nremainingCapacity10mAh=%hu\nnominalCapacity10mAh=%hu\nprotectionBitmask=%hu\nsocPrct=%hhu",
        &latestBMSState.totalVoltage10mV,
        &latestBMSState.current10mAh,
        &latestBMSState.remainingCapacity10mAh,
        &latestBMSState.nominalCapacity10mAh,
        &latestBMSState.protectionBitmask,
        &latestBMSState.socPrct
      );
      latestBMSStateIsValid = result == 6;
      fclose(file);
    }
    
    displayLoop();
    usleep(10000);
    _millis += 10;
  }
}


