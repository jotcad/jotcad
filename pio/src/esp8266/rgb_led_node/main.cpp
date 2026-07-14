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
bool was_connected = false;

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

    // State 1: Booting / Before Connecting to WiFi -> Red
    r_val = 255; g_val = 0; b_val = 0;
    analogWrite(RED_PIN, r_val);
    analogWrite(GREEN_PIN, g_val);
    analogWrite(BLUE_PIN, b_val);
    Serial.println("[ESP8266-RGB] State: Booting. LED: Red");
    delay(500); // Small delay to let the red LED color show clearly

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // State 2: Connecting to WiFi -> Yellow/Orange
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 128); // Yellow-Orange
    analogWrite(BLUE_PIN, 0);
    Serial.print("[ESP8266-RGB] State: Connecting to WiFi. LED: Yellow");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[ESP8266-RGB] WiFi Connected.");
    Serial.printf("[ESP8266-RGB] IP Address: %s\n", WiFi.localIP().toString().c_str());

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // State 3: WiFi Connected, Configuring VFS -> Blue
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 255);
    Serial.println("[ESP8266-RGB] State: WiFi Connected, Configuring VFS. LED: Blue");

    // Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint32_t chipId = ESP.getChipId();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(chipId & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    node = new fs::VFS(config);
    node->begin();

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

    if (node->is_connected()) {
        was_connected = true;
        // State 4: VFS Ready / Listening for control updates -> Green
        r_val = 0; g_val = 255; b_val = 0;
        analogWrite(RED_PIN, r_val);
        analogWrite(GREEN_PIN, g_val);
        analogWrite(BLUE_PIN, b_val);
        Serial.println("[ESP8266-RGB] State: VFS Ready. LED: Green");
        Serial.println("VFS_READY");
    } else {
        was_connected = false;
        // VFS Connection Failed -> Red
        r_val = 255; g_val = 0; b_val = 0;
        analogWrite(RED_PIN, r_val);
        analogWrite(GREEN_PIN, g_val);
        analogWrite(BLUE_PIN, b_val);
        Serial.println("[ESP8266-RGB] State: VFS Connection Failed. LED: Red");
    }
}

void loop() {
    node->tick();

    bool current_connected = node->is_connected();
    if (current_connected != was_connected) {
        was_connected = current_connected;
        if (current_connected) {
            // Reconnected / VFS Ready -> Green
            r_val = 0; g_val = 255; b_val = 0;
            analogWrite(RED_PIN, r_val);
            analogWrite(GREEN_PIN, g_val);
            analogWrite(BLUE_PIN, b_val);
            Serial.println("[ESP8266-RGB] State: VFS Reconnected. LED: Green");
            Serial.println("VFS_READY");
        } else {
            // Connection Lost -> Red
            r_val = 255; g_val = 0; b_val = 0;
            analogWrite(RED_PIN, r_val);
            analogWrite(GREEN_PIN, g_val);
            analogWrite(BLUE_PIN, b_val);
            Serial.println("[ESP8266-RGB] State: VFS Connection Lost. LED: Red");
        }
    }
}
