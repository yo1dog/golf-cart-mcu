#pragma once
#include <cstdint>
#include <cstddef>

#define HIGH 1
#define LOW 0

uint32_t millis();

void digitalWrite(uint8_t pin, bool val);
bool digitalRead(uint8_t pin);

void _setMillis(uint32_t m);
