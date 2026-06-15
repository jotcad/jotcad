# Human Presence Sensor Node (LD2410C) - ESP8266

This node integrates an LD2410C 24GHz mmWave radar sensor into the JotCAD VFS mesh using the ESP8266 (D1 Mini). 

## Hardware Integration

### Stable Hardware Serial Configuration
To maintain stability at high baud rates and prevent Watchdog Timeouts (WDT), this node uses **Hardware UART0** swapped to alternative pins.

| LD2410C Pin | D1 Mini Pin | Function |
| :--- | :--- | :--- |
| **TX** | **D7 (GPIO 13)** | Hardware RX (Swapped) |
| **RX** | **D8 (GPIO 15)** | Hardware TX (Swapped) |
| **VCC** | **5V / VIN** | Power (Must be 5V) |
| **GND** | **GND** | Ground |

### Monitoring (Logs)
Because Hardware Serial is used for the radar, the standard USB monitor is disabled. Debug logs are routed to **Serial1** on pin **D4 (GPIO 2)** at **115200 baud**. 

## ⚠️ Important: Baud Rate Downgrade
The ESP8266 struggles with the factory default baud rate of **256000**. You MUST downgrade the sensor to **115200 baud** using the ESP32 utility before using it with this node.

## VFS Interface
- **Topic**: `sensor/presence`
- **Fulfillment**: `GET /sensor/presence` returns detailed target state and distances.
