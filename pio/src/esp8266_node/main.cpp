#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "vfs.h"
#include "secrets.h"

fs::VFS *node = nullptr;
static uint32_t shared_count = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266] JotCAD VFS Node Booting...");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP8266] IP Address: %s\n", WiFi.localIP().toString().c_str());

    // 1. Setup VFS Node
    fs::VFS::Config config;
#ifdef DEVICE_NODE_ID
    config.id = DEVICE_NODE_ID;
#else
    config.id = "esp8266-real-node-01";
#endif
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    
    // Connect to JS Gateway Node neighbor
    config.neighbors = {MESH_NEIGHBOR_URL};
    
    delay(2000); // Wait for JS neighbor to stabilize
    
    // Bridge Sanity Check
    Serial.println("[ESP8266] Probing host neighbor sanity (GET /health)...");
    WiFiClient client;
    HTTPClient probe;
    probe.begin(client, String(MESH_NEIGHBOR_URL) + "/health");
    int probeCode = probe.GET();
    if (probeCode > 0) {
        Serial.printf("[ESP8266] Neighbor Sanity OK: %d\n", probeCode);
    } else {
        Serial.printf("[ESP8266] Neighbor Sanity FAILED: %s (%d)\n", probe.errorToString(probeCode).c_str(), probeCode);
    }
    probe.end();

    node = new fs::VFS(config);

    // 2. Register a handler to read the current count
    node->register_op("sensor/counter", [](const fs::json& params, fs::VFSRequest *request) {
        request->send(200, "application/json", fs::json({{"value", shared_count}}).dump().c_str());
    }, {{"output", "number"}});

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick(); // Cooperative execution tick
    
    static unsigned long last_tick = 0;
    
    if (millis() - last_tick > 1000) {
        shared_count++;
        last_tick = millis();

        // Broadcast count change
        fs::json selector = {{"path", "sensor/counter"}};
        fs::json payload = {{"value", shared_count}};
        int interested = node->notify(selector, payload);
        
        Serial.printf("[ESP8266] Counter: %d (Interested: %d)\n", shared_count, interested);
    }
}
