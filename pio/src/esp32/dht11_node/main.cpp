#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD DHT11 Sensor Node
 * Hardware:
 * - DHT11 VCC -> 3.3V or 5V
 * - DHT11 GND -> GND
 * - DHT11 DATA -> GPIO 4 (Default)
 */

#ifndef DHT_PIN
#define DHT_PIN 4
#endif
#ifndef DHT_TYPE
#define DHT_TYPE DHT11
#endif

DHT dht(DHT_PIN, DHT_TYPE);
fs::VFS *node = nullptr;

float current_temp = 0;
float current_humidity = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32] DHT11 Sensor Node Booting...");

    dht.begin();

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP32] IP Address: %s\n", WiFi.localIP().toString().c_str());

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};

    node = new fs::VFS(config);

    // Register sensor endpoint
    

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    static unsigned long last_read = 0;
    if (millis() - last_read > 2000) { // DHT11 needs at least 1s between reads
        last_read = millis();

        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t)) {
            Serial.println("[DHT] Failed to read from sensor!");
            return;
        }

        current_temp = t;
        current_humidity = h;

        // Broadcast if changed or heartbeat
        static float last_sent_t = -100;
        static unsigned long last_broadcast = 0;

        if (abs(current_temp - last_sent_t) > 0.5 || (millis() - last_broadcast > 30000)) {
            last_sent_t = current_temp;
            last_broadcast = millis();

            
            fs::json payload = {
                {"temperature", current_temp},
                {"humidity", current_humidity}
            };
            node->publish("sensor/environment", payload);
            Serial.printf("[DHT] Temp: %.1f C, Humidity: %.1f %%\n", current_temp, current_humidity);
        }
    }
}
