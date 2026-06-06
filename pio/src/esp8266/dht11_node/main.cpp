#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHT.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD DHT11 Sensor Node (ESP8266 D1 Mini)
 * Hardware:
 * - DHT11 VCC -> 3.3V or 5V
 * - DHT11 GND -> GND
 * - DHT11 DATA -> D4 (GPIO 2) [Default]
 */

#ifndef DHT_PIN
#define DHT_PIN 2 // D4 on D1 Mini
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
    Serial.println("\n[ESP8266] DHT11 Sensor Node Booting...");

    dht.begin();

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP8266] IP Address: %s\n", WiFi.localIP().toString().c_str());

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint32_t chipId = ESP.getChipId();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(chipId & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};

    node = new fs::VFS(config);

    // Register sensor endpoint
    node->register_op("sensor/environment", [](const fs::json& params, fs::VFSResponseWriter *response) {
        fs::json data = {
            {"temperature", current_temp},
            {"humidity", current_humidity}
        };
        response->send(200, "application/json", data.dump().c_str());
    }, {{"output", "object"}});

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    static unsigned long last_read = 0;
    if (millis() - last_read > 2000) {
        last_read = millis();

        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (isnan(h) || isnan(t)) {
            Serial.println("[DHT] Failed to read from sensor!");
            return;
        }

        current_temp = t;
        current_humidity = h;

        static float last_sent_t = -100;
        static unsigned long last_broadcast = 0;

        if (abs(current_temp - last_sent_t) > 0.5 || (millis() - last_broadcast > 30000)) {
            last_sent_t = current_temp;
            last_broadcast = millis();

            fs::Selector selector("sensor/environment");
            fs::json payload = {
                {"temperature", current_temp},
                {"humidity", current_humidity}
            };
            node->notify(selector, payload);
            Serial.printf("[DHT] Temp: %.1f C, Humidity: %.1f %%\n", current_temp, current_humidity);
        }
    }
}
