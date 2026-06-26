# ESP8266 RGB LED Control Node

This directory contains the ESP8266 VFS client application to control an RGB LED via the Zenoh mesh subscription.

## Responsibility
- Listen for control updates on the `control/rgb` path.
- Map incoming RGB levels (0-255) to hardware PWM output pins (`D2`, `D3`, `D4`).
