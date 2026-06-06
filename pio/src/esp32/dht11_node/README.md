# DHT11 Environmental Sensor Node - ESP32

This node integrates a DHT11 temperature and humidity sensor into the JotCAD VFS mesh.

## Hardware Integration

| DHT11 Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| DATA | GPIO 4 | Data |
| VCC | 3.3V / 5V | Power |
| GND | GND | Ground |

## VFS Interface
- **Topic**: `sensor/environment`
- **Payload**:
  ```json
  {
    "temperature": 22.5,
    "humidity": 45.0
  }
  ```
