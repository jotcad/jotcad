# PlatformIO (PIO) Directory

This directory contains the PlatformIO project for the ESP32 and ESP32-CAM firmware, including scripts for flashing and monitoring serial output.

## Responsibilities
- ESP32 and ESP32-CAM firmware development.
- Interfacing with IoT sensors and mesh subscribers.
- Serial communication flashing and monitoring scripts.

## Contents
- **`src/`**: ESP32 C++ source code.
- **`include/`**: C++ header files.
- **`lib/`**: Local helper libraries.
- **`test/`**: Unit and integration tests for PIO components.
- **`flash_*.sh`**: Scripts to compile and upload firmware via USB serial connection.
- **`monitor_*.sh`**: Scripts to listen to serial output from flashed devices.
- **`ota_flash.sh`**: Unified script to compile and upload firmware Over-The-Air (OTA) to any device on the network (run interactively or via arguments).
- **`platformio.ini`**: PlatformIO environments and dependencies configuration.
