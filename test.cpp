#include <cstdint>
#include <unistd.h>
#include <cstdio>
#include <Arduino.h>

#include "bms.h"
#include "display.h"

#define BMS_FILE_READ_INTERVAL_MS 100

const char*   litChar = "\u2588";
const char* unlitChar = "\u2591";
FILE* bmsFile;

int main() {
  bmsSetup();
  displaySetup();
  
  bmsFile = fopen("sim/bms.txt", "r");
  if (bmsFile == NULL) {
    fputs("Error opening sim/bms.txt\n", stderr);
    return 1;
  }
  
  while (true) {
    bmsLoop();
    bool didUpdateDisplay = displayLoop(bmsIsValid? &bms : NULL);
    
    if (didUpdateDisplay) {
      printf("%u\x1b[0K\n", millis());
      printf("Voltage : %.2fV\x1b[0K\n", bmsIsValid? bms.totalVoltage10mV / 100.0 : 0.0);
      printf("Current : %.2fA\x1b[0K\n", bmsIsValid? bms.current10mAh / 100.0 : 0.0);
      printf("Capacity: %.2f/%.0fAh %.2f%%\x1b[0K\n",
        bmsIsValid? bms.remainingCapacity10mAh / 100.0 : 0.0,
        bmsIsValid? bms.nominalCapacity10mAh / 100.0 : 0.0,
        bmsIsValid && bms.nominalCapacity10mAh > 0? (bms.remainingCapacity10mAh * 100.0) / bms.nominalCapacity10mAh : 0.0
      );
      printf("SoC     : %u%%\x1b[0K\n", bmsIsValid? bms.socPrct : 0);
      printf("Error   : %u\x1b[0K\n", bmsIsValid? bms.protectionBitmask : 0);
      // printf("Anim    : %u %u\x1b[0K\n", animation, frame);
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
      printf("\r\x1b[8A");
    }
    
    usleep(10000);
    _setMillis(_millis + 10);
  }
  
  // Never gets here.
  fclose(bmsFile);
  return 0;
}


class Serial {
  auto printf(auto... args) {
    printf(...args);
  }
}

class Serial2 {
  private:
    uint8_t buffer[256];
    uint8_t endpos = 0;
    uint8_t readpos = 0;
    uint32_t nextFrameInjectionTSMS = -1;
    
    void injectFrame() {
      fflush(bmsFile);
      rewind(bmsFile);
      
      BMSState bms;
      int result = fscanf(
        bmsFile,
        "totalVoltage10mV[ \t]=[ \t]%hu\n"
        "current10mAh=[ \t]-[ \t]%hd\n"
        "remainingCapacity10mAh[ \t]=[ \t]%hu\n"
        "nominalCapacity10mAh[ \t]=[ \t]%hu\n"
        "cycleCount[ \t]=[ \t]%hu\n"
        "productionYear[ \t]=[ \t]%hu\n"
        "productionMonth[ \t]=[ \t]%hhu\n"
        "productionDay[ \t]=[ \t]%hhu\n"
        "cellBalanceStatusBitmask[ \t]=[ \t]%u\n"
        "protectionBitmask[ \t]=[ \t]%hu\n"
        "softwareVersion[ \t]=[ \t]%hhu\n"
        "socPrct[ \t]=[ \t]%hhu\n"
        "chargeMosfet[ \t]=[ \t]%hhu\n"
        "dischargeMosfet[ \t]=[ \t]%hhu\n"
        "cellCount[ \t]=[ \t]%hhu\n"
        "tempSensorCount[ \t]=[ \t]%hhu\n"
        "tempSensors10C[0][ \t]=[ \t]%hd\n"
        "tempSensors10C[1]=[ \t]-[ \t]%hd\n"
        "tempSensors10C[2][ \t]=[ \t]%hd\n"
        "tempSensors10C[3][ \t]=[ \t]%hd\n"
        "tempSensors10C[4][ \t]=[ \t]%hd",
        &bms.totalVoltage10mV,
        &bms.current10mAh,
        &bms.remainingCapacity10mAh,
        &bms.nominalCapacity10mAh,
        &bms.cycleCount,
        &bms.productionYear,
        &bms.productionMonth,
        &bms.productionDay,
        &bms.cellBalanceStatusBitmask,
        &bms.protectionBitmask,
        &bms.softwareVersion,
        &bms.socPrct,
        &bms.chargeMosfet,
        &bms.dischargeMosfet,
        &bms.cellCount,
        &bms.tempSensorCount,
        &bms.tempSensors10C[0],
        &bms.tempSensors10C[1],
        &bms.tempSensors10C[2],
        &bms.tempSensors10C[3],
        &bms.tempSensors10C[4]
      );
      
      if (result == 6) {
        /*
        const uint16_t CELL_OVERVOLTAGE_PROTECTION          = 1 << 0;
        const uint16_t CELL_UNDERVOLTAGE_PROTECTION         = 1 << 1;
        const uint16_t PACK_OVERVOLTAGE_PROTECTION          = 1 << 2;
        const uint16_t PACK_UNDERVOLTAGE_PROTECTION         = 1 << 3;
        const uint16_t CHARGE_OVERTEMPERATURE_PROTECTION    = 1 << 4;
        const uint16_t CHARGE_LOW_TEMPERATURE_PROTECTION    = 1 << 5;
        const uint16_t DISCHARGE_OVERTEMPERATURE_PROTECTION = 1 << 6;
        const uint16_t DISCHARGE_LOW_TEMPERATURE_PROTECTION = 1 << 7;
        const uint16_t CHARGE_OVERCURRENT_PROTECTION        = 1 << 0 << 8;
        const uint16_t DISCHARGE_OVERCURRENT_PROTECTION     = 1 << 1 << 8;
        const uint16_t SHORT_CIRCUIT_PROTECTION             = 1 << 2 << 8;
        const uint16_t FRONT_END_DETECTION_IC_ERROR         = 1 << 3 << 8;
        const uint16_t SOFTWARE_LOCKS_MOS                   = 1 << 4 << 8;
        const uint16_t CHARGE_MOS_BREAKDOWN_FLAG            = 1 << 5 << 8;
        const uint16_t DISCHARGE_MOS_BREAKDOWN_FLAG         = 1 << 6 << 8;
        const uint16_t RESERVED                             = 1 << 7 << 8;
        bms.protectionBitmask = (
          CELL_OVERVOLTAGE_PROTECTION |
          PACK_OVERVOLTAGE_PROTECTION |
          SHORT_CIRCUIT_PROTECTION |
          SOFTWARE_LOCKS_MOS
        );
        */
        
        // We explicitly write to the buffer wrapping around the end and potentially overriding
        // data currently being read.
        buffer[endpos+0] = 0xDD; // start
        buffer[endpos+1] = 0x03; // command
        buffer[endpos+2] = 0x00; // status
        buffer[endpos+3] = 32;   // data size
        write16(buffer, endpos+4+0, bms.totalVoltage10mV);
        write16(buffer, endpos+4+2, (uint16_t)bms.current10mAh);
        write16(buffer, endpos+4+4, bms.remainingCapacity10mAh);
        write16(buffer, endpos+4+6, bms.nominalCapacity10mAh);
        write16(buffer, endpos+4+8, bms.cycleCount);
        write16(buffer, endpos+4+10, (
          (uint16_t)(bms.productionYear - 2000) << 9 |
          (uint16_t)productionMonth << 5 |
          (uint16_t)productionDay
        ));
        write32(buffer, endpos+4+12, state.cellBalanceStatusBitmask);
        write16(buffer, endpos+4+16, state.protectionBitmask);
        buffer[endpos+4+18] = state.softwareVersion;
        buffer[endpos+4+19] = state.socPrct;
        buffer[endpos+4+20] = (
          state.chargeMosfet   ? 0x01 : 0 |
          state.dischargeMosfet? 0x02 : 0
        );
        buffer[endpos+4+21] = state.cellCount;
        buffer[endpos+4+22] = state.tempSensorCount;
        write16(buffer, endpos+4+23, (uint16_t)(state.tempSensors10C[0] + 2731));
        write16(buffer, endpos+4+25, (uint16_t)(state.tempSensors10C[1] + 2731));
        write16(buffer, endpos+4+27, (uint16_t)(state.tempSensors10C[2] + 2731));
        write16(buffer, endpos+4+29, (uint16_t)(state.tempSensors10C[3] + 2731));
        write16(buffer, endpos+4+31, (uint16_t)(state.tempSensors10C[4] + 2731));
        
        uint16_t checksum = 0;
        for (int i = 2; i < 37; ++i) checksum -= buffer[endpos+i];
        write16(buffer, endpos+37, checksum);
        
        buffer[endpos+39] = 0x77; /// stop
        endpos += 40;
        
        if (endpost - readpos < 40) {
          // If we overlapped the read position, move the read position to the start of this frame.
          readpos = endpost - 40;
        }
      }
    }
  
  public:
    auto begin(auto a, auto b, auto c, auto d) {
      // noop
    }
    auto available() {
      // Use available() as a lazy trigger for injecting frames into the buffer.
      if (millis() >= nextFrameInjectionTSMS) {
        nextFrameInjectionTSMS = -1;
        injectFrame();
      }
      
      return endpos - readpos; // Takes advantage of unsigned integer math to handle wrapping.
    }
    auto read() {
      if (readpos == endpos) throw "Attempted to read past end of buffered data."
      return buffer[readpos++]; // Is expected to wrap.
    }
    auto write(auto data, auto size) {
      nextFrameInjectionTSMS = millis() + 5;
    }
}

void write16(uint8_t* buff, uint8_t i, uint16_t val) {
  buff[i  ] = (uint8_t)(val >> 8);
  buff[i+1] = (uint8_t)(val);
}
void write32(uint8_t* buff, uint8_t i, uint32_t val) {
  buff[i  ] = (uint8_t)(val >> 24);
  buff[i+1] = (uint8_t)(val >> 16);
  buff[i+2] = (uint8_t)(val >> 8);
  buff[i+3] = (uint8_t)(val);
}
