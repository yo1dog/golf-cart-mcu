#include "Arduino.h"

uint32_t _millis = 0;
uint32_t millis() {
  return _millis;
}
void _setMillis(uint32_t m) {
  _millis = m;
}

uint32_t _pinBitmap = 0;
void digitalWrite(uint8_t pin, bool val) {
  if (val) {
    _pinBitmap |= 1u << pin;
  }
  else {
    _pinBitmap &= ~(1u << pin);
  }
}
bool digitalRead(uint8_t pin) {
  return _pinBitmap & (1u << pin)? true : false;
}
