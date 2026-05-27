# ESP8266 VFS Node Application

This directory contains the PlatformIO application for an ESP8266-based JotCAD VFS node (e.g., running on a WeMos D1 or NodeMCU board).

## Responsibility

This application initializes Wi-Fi connectivity, performs a handshake with a parent JS gateway node neighbor over HTTP, and operates as an edge participant in the mesh network.

## Key Features

- **Cooperative Event Loop**: Leverages the non-blocking cooperative `tick()` call within the main Arduino `loop()` to process network events without blocking execution.
- **Sensor Counter Example**: Registers a custom CAD service endpoint `sensor/counter` that serves a local counter value and broadcasts updates to interested mesh subscribers every second.

## Files Summary

- [main.cpp](file:///home/brian/github/jotcad/pio/src/esp8266_node/main.cpp): Application entry point, Wi-Fi initialization, mesh handshake, and registration of the counter operator.
