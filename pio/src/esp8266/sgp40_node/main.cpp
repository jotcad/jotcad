#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_SGP40.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD SGP40 VOC Sensor Node (ESP8266 D1 Mini)
 */

Adafruit_SGP40 sgp;
fs::VFS *node = nullptr;

float latest_temp = 25.0;
float latest_hum = 50.0;
bool has_temp = false;
bool has_hum = false;

int32_t current_voc = -1;
char vfs_node_id[64];

void i2c_recovery(int sda, int scl) {
    pinMode(scl, OUTPUT);
    pinMode(sda, INPUT_PULLUP);
    
    // Toggle SCL 9 times to clear the slave's state machine
    for (int i = 0; i < 9; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(5);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
    }
    
    // Send a Stop condition: SDA goes from low to high while SCL is high
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(sda, HIGH);
    delayMicroseconds(5);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266] SGP40 Sensor Node Booting...");

    // Recover I2C bus from potential ROM bootloader noise on D4 (GPIO2)
    Serial.println("[I2C] Performing I2C bus recovery...");
    i2c_recovery(D3, D4); // Try recovering assuming SCL=D4
    i2c_recovery(D4, D3); // Try recovering assuming SCL=D3
    delay(100);

    // Initializing I2C with custom D3 (SDA), D4 (SCL) pins
    Wire.begin(D3, D4);
    delay(500); // Give I2C bus time to stabilize

    if (!sgp.begin()) {
        Serial.println("[SGP40] Sensor not found on SDA=D3, SCL=D4! Running diagnostic scan...");
        
        // Scan 1: SDA=D3, SCL=D4
        Serial.println("[I2C Scanner] Scanning SDA=D3, SCL=D4...");
        bool found_d3_d4 = false;
        byte error, address;
        for (address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            if (error == 0) {
                Serial.printf("[I2C Scanner] Found device at address 0x%02X on SDA=D3, SCL=D4\n", address);
                found_d3_d4 = true;
            }
        }
        
        // Scan 2: SDA=D4, SCL=D3 (Swapped Pins)
        Serial.println("[I2C Scanner] Scanning swapped SDA=D4, SCL=D3...");
        Wire.begin(D4, D3);
        delay(500);
        bool found_d4_d3 = false;
        for (address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            if (error == 0) {
                Serial.printf("[I2C Scanner] Found device at address 0x%02X on swapped pins SDA=D4, SCL=D3\n", address);
                found_d4_d3 = true;
            }
        }

        if (!found_d3_d4 && !found_d4_d3) {
            Serial.println("[I2C Scanner] No I2C devices detected on either D3/D4 or D4/D3 pin combination.");
            Serial.println("[I2C Scanner] Please verify: 1. Sensor power/GND, 2. Pull-up resistors, 3. Pins are D3 & D4.");
        } else if (found_d4_d3 && !found_d3_d4) {
            Serial.println("[I2C Scanner] Alert: Your I2C lines appear to be physically SWAPPED (SDA=D4, SCL=D3).");
        }
        
        while (1) {
            delay(1000);
        }
    }
    Serial.println("[SGP40] Sensor initialized successfully.");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // Setup VFS Node
    fs::VFS::Config config;
    uint32_t chipId = ESP.getChipId();
    snprintf(vfs_node_id, sizeof(vfs_node_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(chipId & 0xFFFFFF));
    config.id = vfs_node_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};

    node = new fs::VFS(config);

    // Register sensor endpoint
    node->register_op("sensor/voc", [](const fs::json& params, fs::VFSResponseWriter *response) {
        response->send(200, "application/json", fs::json({{"value", current_voc}, {"node", vfs_node_id}}).dump().c_str());
    }, {{"output", "number"}});

    // Handle incoming Temp/Humidity notifications for compensation
    node->on_notification("sensor/temperature", [](const fs::Selector& sel, const fs::json& payload) {
        if (payload.contains("value")) {
            latest_temp = payload["value"].get<float>();
            has_temp = true;
            Serial.printf("[SGP40] Received Temp Compensation update: %.1f C\n", latest_temp);
        }
    });

    node->on_notification("sensor/humidity", [](const fs::Selector& sel, const fs::json& payload) {
        if (payload.contains("value")) {
            latest_hum = payload["value"].get<float>();
            has_hum = true;
            Serial.printf("[SGP40] Received Humidity Compensation update: %.1f %%\n", latest_hum);
        }
    });

    node->begin();

    // Subscribe to Temp/Humidity
    node->subscribe(fs::Selector("sensor/temperature"), 0);
    node->subscribe(fs::Selector("sensor/humidity"), 0);

    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    static unsigned long last_read = 0;
    if (millis() - last_read > 1000) { // SGP40 algorithm expects a 1Hz sample rate
        last_read = millis();

        int32_t voc;
        if (has_temp && has_hum) {
            voc = sgp.measureVocIndex(latest_temp, latest_hum);
        } else {
            voc = sgp.measureVocIndex();
        }

        if (voc < 0) {
            Serial.println("[SGP40] Measurement failed!");
            return;
        }

        current_voc = voc;
        Serial.printf("[SGP40] Read - VOC Index: %d (Compensation: T=%.1f C, H=%.1f %%)\n", current_voc, latest_temp, latest_hum);

        static int32_t last_sent_voc = -100;
        static unsigned long last_broadcast = 0;

        bool force = (millis() - last_broadcast > 30000);

        if (abs((int)current_voc - (int)last_sent_voc) > 5 || force) {
            last_sent_voc = current_voc;
            last_broadcast = millis();
            fs::json params = {{"node", vfs_node_id}};
            int sent_count = node->notify(fs::Selector("sensor/voc", params), {{"value", current_voc}});
            Serial.printf("[SGP40] Published VOC (Sent to %d peers): %d\n", sent_count, current_voc);
        }
    }

    // Periodically re-subscribe to maintain mesh interests
    static unsigned long last_sub = 0;
    if (millis() - last_sub > 30000) {
        last_sub = millis();
        node->subscribe(fs::Selector("sensor/temperature"), 0);
        node->subscribe(fs::Selector("sensor/humidity"), 0);
    }
}
