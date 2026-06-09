# Microcontroller for Golf Cart

Primary function is to read lithium battery's BMS and drive a 10 segment LED bar display.

```sh
g++ -std=c++20 -Isim test.cpp display.cpp sim/Arduino.cpp -o dist/test && ./dist/test
```

```
3334400
Voltage : 53.28V
Current : 0.08A
Capacity: 89.21/120Ah 74.34%
SoC     : 74%
Error   : 0
[░░░░░░░█░░] ░░░
 0123456789  bat
 ```
 