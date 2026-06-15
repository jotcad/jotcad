# Human Presence Sensor Node (LD2410C)

This node integrates an LD2410C 24GHz mmWave radar sensor into the JotCAD VFS mesh. It provides high-sensitivity human presence detection, including both moving and stationary targets.

## Hardware Integration

### ESP32-C3 Super Mini (Target: `esp32c3_human_presence`)
| LD2410C Pin | ESP32-C3 Pin | Function |
| :--- | :--- | :--- |
| TX | GPIO 20 | UART RX |
| RX | GPIO 21 | UART TX |
| VCC | 5V / VCC | Power |
| GND | GND | Ground |

### Standard ESP32 / NodeMCU (Target: `esp32_human_presence`)
| LD2410C Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| TX | GPIO 16 (RX2) | UART RX |
| RX | GPIO 17 (TX2) | UART TX |
| VCC | VIN / 5V | Power |
| GND | GND | Ground |

## ⚠️ Critical Power Requirements

**Power Consumption**: The LD2410C radar is power-hungry and produces significant current spikes during initialization and ranging. 

**Symptoms of Poor Power**:
- `flash read err, 1000` on boot.
- `TG0WDT_SYS_RESET` or `SW_CPU_RESET` loops.
- VFS disconnection during detection events.

**Mandatory Stabilizations**:
1. **Use 5V**: Always power the sensor from a 5V rail (VIN/VCC). Powering from 3.3V is insufficient and will cause instability.
2. **Decoupling Capacitor**: It is highly recommended to add a **100µF - 470µF electrolytic capacitor** across the 5V and GND rails as close to the sensor/ESP32 as possible to buffer current spikes.
3. **High-Current Source**: Ensure your USB source or power adapter can provide at least 1A. Standard laptop USB 2.0 ports (500mA) may be insufficient for both the ESP32 WiFi burst and the Radar pulse simultaneously.

## Baud Rate Downgrade Utility

The default baud rate of the LD2410C is **256000**, which can be unstable on ESP8266 devices. This sketch includes a utility in `setup()` to permanently downgrade the sensor to **115200** baud. 

**Usage**:
1. Flash this project to an ESP32.
2. Watch the Serial Monitor for `[SUCCESS] Radar is now talking at 115200 baud!`.
3. The setting is persistent in the sensor's Flash memory.

## VFS Interface

- **Topic**: `sensor/presence`
- **Payload**:
  ```json
  {
    "state": 1,          // 0: None, 1: Moving, 2: Stationary, 3: Both
    "distance_cm": 150   // Distance to the primary target
  }
  ```
- **Fulfillment**: `GET /sensor/presence` returns a detailed object including energy levels for both moving and static targets.
