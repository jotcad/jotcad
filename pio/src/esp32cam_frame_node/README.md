# ESP32-CAM Camera Frame Node Application

This directory contains the PlatformIO application for an ESP32-CAM board acting as a JotCAD VFS node.

## Responsibility

This application initializes Wi-Fi, configures the ESP32-CAM camera module, connects to the JotCAD VFS mesh, and serves live images and camera frames as VFS assets.

## Key Features

- **VFS Image Assets**: Exposes a camera capture operator through the VFS structure to allow external mesh clients to request camera frames.
- **Dynamic Framing**: Leverages the ESP32-CAM hardware drivers to capture and encode camera frames into JPEGs, served dynamically over VFS.

## Files Summary

- [main.cpp](file:///home/brian/github/jotcad/pio/src/esp32cam_frame_node/main.cpp): Configures the camera sensor, registers VFS handlers for capturing and retrieving images, and tick-loops the VFS client.
