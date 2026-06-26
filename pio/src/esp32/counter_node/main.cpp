#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

fs::VFS *node = nullptr;
static uint32_t shared_count = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32] JotCAD VFS Node Booting...");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP32] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[ESP32] Gateway:    %s\n", WiFi.gatewayIP().toString().c_str());

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined! Please define sysenv.DEVICE_NODE_ID in your environment."
#endif
static_assert(sizeof(DEVICE_NODE_ID) > 2, "DEVICE_NODE_ID cannot be empty! Please define sysenv.DEVICE_NODE_ID in your environment.");

    // 1. Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    
    // Use the host's actual LAN IP
    config.neighbors = {MESH_NEIGHBOR_URL};
    
    delay(2000); // Wait for JS neighbor to stabilize
    
    // Bridge Sanity Check
    Serial.println("[ESP32] Probing host neighbor sanity (GET /health)...");
    HTTPClient probe;
    probe.begin(String(MESH_NEIGHBOR_URL) + "/health");
    int probeCode = probe.GET();
    if (probeCode > 0) {
        Serial.printf("[ESP32] Neighbor Sanity OK: %d\n", probeCode);
    } else {
        Serial.printf("[ESP32] Neighbor Sanity FAILED: %s (%d)\n", probe.errorToString(probeCode).c_str(), probeCode);
    }
    probe.end();

    node = new fs::VFS(config);



    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();
    
    static unsigned long last_tick = 0;
    
    if (millis() - last_tick > 1000) {
        shared_count++;
        last_tick = millis();

        // Broadcast count change
        fs::json payload = {{"value", shared_count}};
        node->publish("sensor/counter", payload);
        
        Serial.printf("[ESP32] Counter: %d\n", shared_count);
    }
}
