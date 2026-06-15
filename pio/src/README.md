# PlatformIO Firmware Source Directory

This directory contains the firmware source code for the JotCAD C++ VFS Node clients, organized by hardware target platforms and specific applications.

## High-Level Summary of Sub-directories

- **[esp32](file:///home/brian/github/jotcad/pio/src/esp32)**: The core VFS client library implementation for ESP32 microcontrollers, utilizing `mbedtls` and multi-threaded synchronization.
- **[esp32/human_presence_node](file:///home/brian/github/jotcad/pio/src/esp32/human_presence_node)**: VFS presence detection node (LD2410C radar) with persistent configuration utilities.
- **[esp32/dht11_node](file:///home/brian/github/jotcad/pio/src/esp32/dht11_node)**: Environmental monitoring node (Temperature/Humidity).
- **[esp8266](file:///home/brian/github/jotcad/pio/src/esp8266)**: Custom single-core cooperative VFS client library implementation for ESP8266 microcontrollers, utilizing native `BearSSL` for hashing.
- **[esp8266/human_presence_node](file:///home/brian/github/jotcad/pio/src/esp8266/human_presence_node)**: Optimized radar node for ESP8266 using hardware UART swapping for stability at high baud rates.
- **[esp8266/dht11_node](file:///home/brian/github/jotcad/pio/src/esp8266/dht11_node)**: Lightweight DHT11 environmental node for ESP8266.
