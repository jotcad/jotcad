#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD Human Presence Node (LD2410C + ESP8266 D1 Mini)
 * 
 * Hardware Serial (D7/D8) @ 115200 Baud
 * Monitor Hardware Serial1 (D4) @ 115200 Baud
 */

#define LD2410_BAUD 115200
#define MONITOR_BAUD 115200

// Use Serial1 (D4) for all debug logging
#define DEBUG_LOG(x) Serial1.println(x)
#define DEBUG_LOG_F(fmt, ...) Serial1.printf(fmt, ##__VA_ARGS__)

fs::VFS *node = nullptr;

struct RadarState {
    uint8_t target_state;
    uint16_t moving_dist;
    uint16_t static_dist;
    uint16_t detect_dist;
};

RadarState current_radar = {0};

const char* get_state_name(uint8_t state) {
    switch (state) {
        case 0: return "None";
        case 1: return "Moving";
        case 2: return "Stationary";
        case 3: return "Both";
        default: return "Unknown";
    }
}

void parse_ld2410(uint8_t *buffer, size_t len) {
    if (len < 12 || buffer[0] != 0x02) return;
    current_radar.target_state = buffer[1];
    current_radar.moving_dist = buffer[2] | (buffer[3] << 8);
    current_radar.static_dist = buffer[5] | (buffer[6] << 8);
    current_radar.detect_dist = buffer[8] | (buffer[9] << 8);
}

void setup() {
    Serial1.begin(MONITOR_BAUD);
    delay(1000);
    DEBUG_LOG("\n[ESP8266] Human Presence Node Booting...");

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial1.print(".");
    }
    DEBUG_LOG("\nWiFi Connected.");

    // Switch to Hardware Serial on D7/D8
    Serial.flush();
    Serial.begin(LD2410_BAUD);
    Serial.swap();

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
    node->begin();
    DEBUG_LOG("VFS_READY");
}

void loop() {
    node->tick();
    yield();

    static uint8_t rx_buffer[128];
    static int rx_idx = 0;
    static bool frame_started = false;
    static uint32_t total_bytes = 0;
    static unsigned long last_byte_seen = 0;

    while (Serial.available()) {
        uint8_t b = Serial.read();
        total_bytes++;
        last_byte_seen = millis();

        if (rx_idx < 128) rx_buffer[rx_idx++] = b;
        else rx_idx = 0;
        
        if (!frame_started && rx_idx >= 4) {
            if (rx_buffer[rx_idx-4] == 0xF4 && rx_buffer[rx_idx-3] == 0xF3 && 
                rx_buffer[rx_idx-2] == 0xF2 && rx_buffer[rx_idx-1] == 0xF1) {
                rx_idx = 4;
                frame_started = true;
            } else if (rx_idx >= 8) {
                memmove(rx_buffer, rx_buffer + 1, --rx_idx);
            }
        }
        
        if (frame_started && rx_idx >= 10) {
            if (rx_buffer[rx_idx-4] == 0xF8 && rx_buffer[rx_idx-3] == 0xF7 && 
                rx_buffer[rx_idx-2] == 0xF6 && rx_buffer[rx_idx-1] == 0xF5) {
                uint16_t payload_len = rx_buffer[4] | (rx_buffer[5] << 8);
                if (rx_idx == payload_len + 10) {
                    parse_ld2410(&rx_buffer[6], payload_len);
                    static uint8_t last_state = 255;
                    if (current_radar.target_state != last_state) {
                        last_state = current_radar.target_state;
                        fs::Selector selector("sensor/presence");
                        fs::json payload = {{"state", current_radar.target_state}, {"distance_cm", current_radar.detect_dist}};
                        node->notify(selector, payload);
                        DEBUG_LOG_F("[RADAR] State: %s (%d), Dist: %d cm\n", get_state_name(current_radar.target_state), current_radar.target_state, current_radar.detect_dist);
                    }
                }
                rx_idx = 0;
                frame_started = false;
            }
        }
    }

    static unsigned long last_status = 0;
    if (millis() - last_status > 5000) {
        last_status = millis();
        if (total_bytes > 0) {
            DEBUG_LOG_F("[DEBUG] Bytes: %u (Last: %lu ms ago)\n", total_bytes, millis() - last_byte_seen);
        } else {
            DEBUG_LOG("[DEBUG] No bytes received. Check D7/D8.");
        }
    }
}
