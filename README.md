# Microcontroller for Golf Cart

Primary function is to read lithium battery's BMS and drive a 10 segment LED bar display.

Basic simulator for local itteration. `sim/bms.txt` can be modified realtime to simulate the BMS state.
```sh
g++ -std=c++20 -Wall -Wextra -Isim sim/sim.cpp bms.cpp display.cpp sim/Arduino.cpp sim/bmsSimulator.cpp -o dist/sim && ./dist/sim
```

```
Read Age: 1s
Voltage : 53.28V
Current : 8.00A
Capacity: 89.21/120Ah 74.34%
SoC     : 74%
Error   : 0
[░░░░░░░█░░] ░░░
 0123456789  bat
 ```
 