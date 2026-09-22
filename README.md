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