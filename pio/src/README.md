# PlatformIO Firmware Source Directory

This directory contains the firmware source code for the JotCAD C++ VFS Node clients, organized by hardware target platforms and specific applications.

## High-Level Summary of Sub-directories

- **[esp32](file:///home/brian/github/jotcad/pio/src/esp32)**: The core VFS client library implementation for ESP32 microcontrollers, utilizing `mbedtls` and multi-threaded synchronization.
- **[esp32dev_counter_node](file:///home/brian/github/jotcad/pio/src/esp32dev_counter_node)**: VFS sensor counter node application for standard ESP32 development boards.
- **[esp32cam_frame_node](file:///home/brian/github/jotcad/pio/src/esp32cam_frame_node)**: VFS camera frame stream node application for ESP32-CAM boards, exposing camera frames as VFS assets.
- **[esp8266](file:///home/brian/github/jotcad/pio/src/esp8266)**: Custom single-core cooperative VFS client library implementation for ESP8266 microcontrollers, utilizing native `BearSSL` for hashing.
- **[esp8266_counter_node](file:///home/brian/github/jotcad/pio/src/esp8266_counter_node)**: VFS sensor counter node application for ESP8266 (e.g., WeMos D1/NodeMCU) boards.
