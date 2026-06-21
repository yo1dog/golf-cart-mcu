#pragma once
#include <cstdint>
#include <cstddef>

#define LOW 0
#define HIGH 1

#define INPUT 0
#define OUTPUT 1

#define SERIAL_8N1 0x06

uint32_t millis();
void __sim_setMillis(uint32_t simMillis);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);

class ConsoleSerial {
  public:
    void begin(unsigned long baud);
    int printf(const char* fmt, ...);
};

class HardwareSerial {
  public:
    void begin(unsigned long baud, uint32_t config, int rxPin, int txPin);
    
    int available();
    int read();
    size_t write(const uint8_t* data, size_t size);
    
    int __sim_txAvailable();
    int __sim_txRead();
    size_t __sim_rxWrite(const uint8_t* data, size_t size);
    
  private:
    // Buffers are ring (wrap around) buffers and we rely on unsigned integer math to handle the
    // wrapping (modulo).
    // Because we push the tail 1 byte ahead of the head as we write, the effective buffer size is
    // actually 255.
    uint8_t rxBuffer[256];
    uint8_t rxHead = 0;
    uint8_t rxTail = 0;
    bool rxHadFirstByte = false;
    uint8_t txBuffer[256];
    uint8_t txHead = 0;
    uint8_t txTail = 0;
    bool txHadFirstByte = false;
};

extern ConsoleSerial Serial;
extern HardwareSerial Serial2;
