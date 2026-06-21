#include "Arduino.h"
#include <cstdarg>
#include <cstdio>

static uint32_t __sim_millis = 0;
uint32_t millis() {
  return __sim_millis;
}
void __sim_setMillis(uint32_t millis) {
  __sim_millis = millis;
}

static uint32_t __sim_pinBitmap = 0;
void pinMode(uint8_t /*pin*/, uint8_t /*mode*/) {/*noop*/}
void digitalWrite(uint8_t pin, uint8_t val) {
  if (val) __sim_pinBitmap |= 1u << pin;
  else __sim_pinBitmap &= ~(1u << pin);
}
int digitalRead(uint8_t pin) {
  return __sim_pinBitmap & (1u << pin)? 1 : 0;
}

void ConsoleSerial::begin(unsigned long /*baud*/) {/*noop*/}
int ConsoleSerial::printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vprintf(fmt, args);
  va_end(args);
  return n;
}

void HardwareSerial::begin(unsigned long /*baud*/, uint32_t /*config*/, int /*rxPin*/, int /*txPin*/) {/*noop*/}
int HardwareSerial::available() {
  return (uint8_t)(rxHead - rxTail); 
}
int HardwareSerial::read() {
  if (rxHead == rxTail) return -1;
  return rxBuffer[rxTail++];
}
size_t HardwareSerial::write(const uint8_t* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    txBuffer[txHead++] = data[i];
    if (txHead == txTail) ++txTail;
  }
  return size;
}
int HardwareSerial::__sim_txAvailable() {
  return (uint8_t)(txHead - txTail); 
}
int HardwareSerial::__sim_txRead() {
  if (txHead == txTail) return -1;
  return txBuffer[txTail++];
}
size_t HardwareSerial::__sim_rxWrite(const uint8_t* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    rxBuffer[rxHead++] = data[i];
    if (rxHead == rxTail) ++rxTail;
  }
  return size;
}

ConsoleSerial Serial;
HardwareSerial Serial2;
