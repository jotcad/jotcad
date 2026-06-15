# SGP40 Gas & VOC Index Sensor Node - ESP8266

This node integrates an SGP40 sensor (measuring VOC Index) into the JotCAD VFS mesh.

## Hardware Integration

| SGP40 Pin | D1 Mini Pin | Function |
| :--- | :--- | :--- |
| SDA | **D3 (GPIO 0)** | I2C SDA |
| SCL | **D4 (GPIO 2)** | I2C SCL |
| VCC | 3.3V / 5V | Power |
| GND | GND | Ground |

## VFS Interface
- **Topics**: 
  - `sensor/voc`: VOC (Volatile Organic Compounds) index from 0 to 500.
- **Payloads**:
  - `sensor/voc` -> `{"value": 100, "node": "esp8266-sgp40-XXXXXX"}`
