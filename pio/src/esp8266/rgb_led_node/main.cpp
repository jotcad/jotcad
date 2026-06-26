#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "vfs.h"
#include "secrets.h"

// RGB LED hardware pins (Wemos D1 Mini)
// Red = D2 (GPIO4)
// Green = D3 (GPIO0)
// Blue = D4 (GPIO2)
const int RED_PIN = D2;
const int GREEN_PIN = D3;
const int BLUE_PIN = D4;

fs::VFS *node = nullptr;

// Current state variables
int r_val = 0;
int g_val = 0;
int b_val = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266-RGB] RGB LED Control Node Booting...");

    // Setup hardware pins
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);

    // Set PWM resolution to 8-bit (0-255) matching standard RGB values
    analogWriteRange(255);
    analogWrite(RED_PIN, r_val);
    analogWrite(GREEN_PIN, g_val);
    analogWrite(BLUE_PIN, b_val);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP8266-RGB] IP Address: %s\n", WiFi.localIP().toString().c_str());

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

    // Subscribe to Zenoh control channel: control/rgb
    node->subscribe("control/rgb", [](const fs::json& payload) {
        Serial.println("[ESP8266-RGB] Received control update!");
        
        r_val = payload.value("r", payload.value("red", 0));
        g_val = payload.value("g", payload.value("green", 0));
        b_val = payload.value("b", payload.value("blue", 0));

        // Constrain values to 0-255 range
        r_val = constrain(r_val, 0, 255);
        g_val = constrain(g_val, 0, 255);
        b_val = constrain(b_val, 0, 255);

        // Apply PWM to pins
        analogWrite(RED_PIN, r_val);
        analogWrite(GREEN_PIN, g_val);
        analogWrite(BLUE_PIN, b_val);

        Serial.printf("[ESP8266-RGB] Applied levels -> R: %d, G: %d, B: %d\n", r_val, g_val, b_val);
    });

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();
}
