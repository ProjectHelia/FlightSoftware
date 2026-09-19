# On Board Computer
This repository contains all the code that will be ran on the on board computer system. The OBC is made up on 6 ESP32 S3 microcontrollers, one for each subsystem.

Within the `src/` there is a folder for each subsystem:
- `OBDH-TTC`: Main controller, triggers in-flight events, telemetry, and on-board storage handler.
- `EPS`: Managed power, current and voltage sensors, and triggers power cycles.
- `Instrumentation`: Handles all housekeeping sensors.
- `Mechanisms`: Handles the fluidics and iris actuators.
- `Photonics`: Manages all photodiodes and LEDs for the on board cell excitation period.
- `Thermals`: Contains the thermal controler system (PID) and a kalman filter.

Each ESP32 is connected by a CAN Bus for bi-directional communication. The code to send/receive data over the CAN Bus is avaliable in `libs/libcanbus` which provides the necessary abstractions for communication.

# Contributing 
The repository is licenced under GPLv3 and is available to be used by any team or individual for future REXUSBEXUS, or other programme/competitions.

[text](libs) [text](libs/can) [text](libs/can/can.c) [text](libs/can/can.h) [text](libs/can/protocol.h) [text](libs/fsm) [text](libs/fsm/fsm.h) [text](libs/utilities) [text](src) [text](src/EPS) [text](src/EPS/HAL) [text](src/EPS/HAL/esp32) [text](src/EPS/HAL/esp32/hal_esp32.c) [text](src/EPS/HAL/sim) [text](src/EPS/HAL/sim/hal_sim.c) [text](src/EPS/comm.c) [text](src/EPS/core.c) [text](src/EPS/setup.c) [text](src/Instrumentation) [text](src/Instrumentation/HAL) [text](src/Instrumentation/HAL/esp32) [text](src/Instrumentation/HAL/esp32/hal_esp32.c) [text](src/Instrumentation/HAL/sim) [text](src/Instrumentation/HAL/sim/hal_sim.c) [text](src/Instrumentation/comm.c) [text](src/Instrumentation/core.c) [text](src/Instrumentation/setup.c) [text](src/Master) [text](src/Master/HAL) [text](src/Master/HAL/esp32) [text](src/Master/HAL/esp32/hal_esp32.c) [text](src/Master/HAL/sim) [text](src/Master/HAL/sim/hal_sim.c) [text](src/Master/comm.c) [text](src/Master/core.c) [text](src/Master/setup.c) [text](src/Mechanism) [text](src/Mechanism/HAL) [text](src/Mechanism/HAL/esp32) [text](src/Mechanism/HAL/esp32/hal_esp32.c) [text](src/Mechanism/HAL/sim) [text](src/Mechanism/HAL/sim/hal_sim.c) [text](src/Mechanism/comm.c) [text](src/Mechanism/core.c) [text](src/Mechanism/setup.c) [text](src/Photonics) [text](src/Photonics/HAL) [text](src/Photonics/HAL/esp32) [text](src/Photonics/HAL/esp32/hal_esp32.c) [text](src/Photonics/HAL/sim) [text](src/Photonics/HAL/sim/hal_sim.c) [text](src/Photonics/comm.c) [text](src/Photonics/core.c) [text](src/Photonics/setup.c) [text](src/Thermals) [text](src/Thermals/HAL) [text](src/Thermals/HAL/esp32) [text](src/Thermals/HAL/esp32/hal_esp32.c) [text](src/Thermals/HAL/sim) [text](src/Thermals/HAL/sim/hal_sim.c) [text](src/Thermals/comm.c) [text](src/Thermals/core.c) [text](src/Thermals/setup.c) [text](tests) [text](.gitignore) [text](CMakeLists.txt) [text](LICENSE) [text](platformio.ini) [text](README.md)