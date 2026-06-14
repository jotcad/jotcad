#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHT.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD DHT11 Sensor Node (ESP8266 D1 Mini)
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
char vfs_node_id[64];

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

    // Register sensor endpoints
    node->register_op("sensor/temperature", [](const fs::json& params, fs::VFSResponseWriter *response) {
        response->send(200, "application/json", fs::json({{"value", current_temp}, {"node", vfs_node_id}}).dump().c_str());
    }, {{"output", "number"}});

    node->register_op("sensor/humidity", [](const fs::json& params, fs::VFSResponseWriter *response) {
        response->send(200, "application/json", fs::json({{"value", current_humidity}, {"node", vfs_node_id}}).dump().c_str());
    }, {{"output", "number"}});

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
        Serial.printf("[DHT] Read - Temp: %.1f C, Hum: %.1f %%\n", current_temp, current_humidity);

        static float last_sent_t = -100;
        static float last_sent_h = -100;
        static unsigned long last_broadcast = 0;

        bool force = (millis() - last_broadcast > 30000);
        
        if (abs(current_temp - last_sent_t) > 0.5 || force) {
            last_sent_t = current_temp;
            last_broadcast = millis();
            fs::json params = {{"node", vfs_node_id}};
            int sent_count = node->notify(fs::Selector("sensor/temperature", params), {{"value", current_temp}});
            Serial.printf("[DHT] Published Temp (Sent to %d peers): %.1f C\n", sent_count, current_temp);
        }

        if (abs(current_humidity - last_sent_h) > 2.0 || force) {
            last_sent_h = current_humidity;
            last_broadcast = millis();
            fs::json params = {{"node", vfs_node_id}};
            int sent_count = node->notify(fs::Selector("sensor/humidity", params), {{"value", current_humidity}});
            Serial.printf("[DHT] Published Humidity (Sent to %d peers): %.1f %%\n", sent_count, current_humidity);
        }
    }
}
