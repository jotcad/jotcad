#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TM1637Display.h>
#include "vfs.h"
#include "secrets.h"

// TM1637 Pins (Wemos D1 Mini)
#define CLK D3 // GPIO0
#define DIO D4 // GPIO2

TM1637Display display(CLK, DIO);
fs::VFS *node = nullptr;
static uint32_t shared_count = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266] TM1637 Display Node Booting...");

    display.setBrightness(0x0f);
    display.showNumberDec(0, true);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined! Please define sysenv.DEVICE_NODE_ID in your environment."
#endif

    // 1. Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint32_t chipId = ESP.getChipId();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(chipId & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    
    node = new fs::VFS(config);

    // 2. Register handler for on-demand counter reads
    node->register_op("sensor/counter", [](const fs::json& params, fs::VFSResponseWriter *response) {
        response->send(200, "application/json", fs::json({{"value", shared_count}}).dump().c_str());
    }, {{"output", "number"}});

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    static unsigned long last_tick = 0;
    if (millis() - last_tick > 1000) {
        shared_count++;
        last_tick = millis();

        // Update display
        display.showNumberDec(shared_count % 10000, true);

        // Periodically log mesh status
        if (shared_count % 5 == 0) {
            node->log_status();
        }

        // Broadcast count change
        fs::Selector selector("sensor/counter");
        fs::json payload = {{"value", shared_count}};
        int sent = node->notify(selector, payload);
        
        Serial.printf("[ESP8266] Counter: %d (Sent to: %d)\n", shared_count, sent);
    }
}
