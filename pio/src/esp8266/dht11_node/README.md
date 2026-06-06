# DHT11 Environmental Sensor Node - ESP8266

This node integrates a DHT11 temperature and humidity sensor into the JotCAD VFS mesh.

## Hardware Integration

| DHT11 Pin | D1 Mini Pin | Function |
| :--- | :--- | :--- |
| DATA | **D4 (GPIO 2)** | Data |
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
