# ESP32 Development Counter Node Application

This directory contains the PlatformIO application for a standard ESP32 development board (e.g., ESP32-WROOM-32D) acting as a JotCAD VFS node.

## Responsibility

This application establishes Wi-Fi, performs the VFS handshake with a parent JS gateway node, and participates in the mesh network.

## Key Features

- **Standard VFS Integrations**: Uses the ESP32 VFS library (`pio/src/esp32`) supporting hardware-accelerated SHA-256 and multi-threading options.
- **Sensor Counter Example**: Sets up the `sensor/counter` dynamic endpoint and publishes periodic counter updates to the mesh.

## Files Summary

- [main.cpp](file:///home/brian/github/jotcad/pio/src/esp32dev_counter_node/main.cpp): Core node application implementation, registering operators and driving the event loop.
