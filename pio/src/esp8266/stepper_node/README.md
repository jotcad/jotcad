# ESP8266 Stepper Motor Node

This directory contains the firmware component for reading and controlling a 28BYJ-48 stepper motor with a ULN2003 driver board over the Zenoh-Pico VFS mesh.

## Directory Responsibility
Provides non-blocking stepper driving, coil release safety to prevent motor overheating, state publication, and command subscription.

## Hardware Pinout & Wiring

| ULN2003 Driver Pin | ESP8266 (D1 Mini) Pin | Description |
| :--- | :--- | :--- |
| **GND (-)** | GND | Power Ground |
| **VCC (+)** | 5V / VBUS | 5V Power Input (Required for motor torque, do NOT use 3.3V) |
| **IN1** | **D1 (GPIO 5)** | Control Coil A |
| **IN2** | **D2 (GPIO 4)** | Control Coil B |
| **IN3** | **D5 (GPIO 14)** | Control Coil C |
| **IN4** | **D6 (GPIO 12)** | Control Coil D |

*Note: D1, D2, D5, and D6 are completely safe pins on ESP8266 that do not pull high or low during boot cycles, preventing startup lockups.*

## Network Interface & Schema

### 1. Command Topic (`actuator/stepper/target`)
The node listens for control payloads on this topic to trigger movement:
```json
{
  "steps": 2048,     // Moves relative to current position if relative=true (+ = CW, - = CCW)
  "angle": 180.0,    // Alternative: Target angle in degrees
  "relative": true,  // If false, targets absolute steps/angle relative to boot origin
  "speed": 1200      // Microsecond delay between step changes (min 900)
}
```

### 2. State Topic (`actuator/stepper/state`)
The node publishes its coordinates upon reaching a target, and is queryable at `jot/vfs/op/actuator/stepper/state`:
```json
{
  "position_steps": 2048,
  "angle_degrees": 180.0,
  "is_moving": false
}
```

## Flashing
Run the flashing command from the project root:
```bash
./pio/flash_esp8266_stepper.sh
```
