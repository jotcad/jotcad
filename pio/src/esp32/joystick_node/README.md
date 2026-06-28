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

## Technical Design & Wiring Constraints

### ⚠️ Powering from 3.3V (VBUS Warning)
Always power the joystick potentiometer module from the ESP32's **`3.3V`** pin, **never from `VBUS` (5V)**. 
* The ESP32-S2/S3 is **not 5V tolerant** on its GPIO pins. 
* Powering via 5V feeds excessive voltage into the ADC inputs, triggering internal ESD protection clamping diodes. This leaks current back into the 3.3V rail and causes severe crosstalk/axis bleed (e.g. Y movement modulating the X value).

### ⚡ ADC Resolution & pinMode
* **12-bit Override**: The ESP32 Arduino framework defaults to 13-bit resolution (0-8191) on ESP32-S2/S3. The firmware explicitly overrides this to 12-bit (`analogReadResolution(12)`) to output coordinates scaled to `0 - 4095`.
* **No analog pinMode**: Do not call `pinMode(pin, INPUT)` on the VRx and VRy analog pins. This can override the ADC multiplexer or enable digital pull-ups, which pulls the analog voltage high.

### 🔄 ADC Crosstalk Prevention
Because the ESP32 shares a single internal sample-and-hold capacitor across its ADC channels, reading two high-impedance potentiometers consecutively can lead to charge bleed. The firmware mitigates this by:
1. Reading each analog pin twice and discarding the first read.
2. Adding a small `delayMicroseconds(50)` between the X and Y channel readings to give the sampling capacitor time to settle.
