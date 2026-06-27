# ESP32 Joystick Node

This directory contains the firmware component for reading a 2-axis analog joystick with a press button and broadcasting its coordinates over the JotCAD Zenoh-Pico VFS mesh.

## Directory Responsibility
Provides input acquisition, threshold filtering, state publication, and connection management for joystick peripherals.

## Hardware Pinout & Wiring

| Joystick Pin | ESP32-S2 / S3 Pin | Function Type | Description |
| :--- | :--- | :--- | :--- |
| **GND** | GND | Power Ground | Power return path |
| **+5V / +3.3V** | 3.3V | Power Supply | Input reference voltage |
| **VRx (X-Axis)** | **GPIO 3** | Analog Input (`ADC1_CH2`) | Horizontal axis voltage |
| **VRy (Y-Axis)** | **GPIO 5** | Analog Input (`ADC1_CH4`) | Vertical axis voltage |
| **SW (Switch)** | **GPIO 7** | Digital Input (`INPUT_PULLUP`) | Press button (active-low) |

*Note: GPIO 3, 5, and 7 are standard odd-numbered pins that do not interfere with ESP32-S2 or ESP32-S3 bootstrap states, ensuring clean board boot cycles.*

## Network Payload
The node publishes dynamic coordinates to the **`sensor/joystick`** VFS topic under the following JSON structure:

```json
{
  "x": 2048,     // Raw 12-bit ADC value (0 - 4095)
  "y": 2048,     // Raw 12-bit ADC value (0 - 4095)
  "pressed": false // Digital state (true = pressed, false = idle)
}
```

## Flashing & Monitoring
Choose the command corresponding to your target board:

### For ESP32-S2 Mini:
```bash
./flash_esp32s2_joystick.sh
```

### For ESP32-S3 SuperMini:
```bash
./flash_esp32s3_joystick.sh
```
