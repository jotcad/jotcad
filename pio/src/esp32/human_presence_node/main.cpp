#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

/**
 * JotCAD Human Presence Node
 * Supports: ESP32-C3 Super Mini, ESP32-WROOM-32
 */

// Hardware Defaults if not provided by build flags
#ifndef RADAR_RX_PIN
#define RADAR_RX_PIN 20
#endif
#ifndef RADAR_TX_PIN
#define RADAR_TX_PIN 21
#endif
#ifndef RADAR_SERIAL
#define RADAR_SERIAL Serial1
#endif
#ifndef LD2410_BAUD
#define LD2410_BAUD 256000
#endif

fs::VFS *node = nullptr;

struct RadarState {
    uint8_t target_state; // 0: None, 1: Moving, 2: Stationary, 3: Both
    uint16_t moving_dist;
    uint8_t moving_energy;
    uint16_t static_dist;
    uint8_t static_energy;
    uint16_t detect_dist;
};

RadarState current_radar = {0};

void parse_ld2410(uint8_t *buffer, size_t len) {
    if (len < 13) return;
    if (buffer[0] != 0x02) return;

    current_radar.target_state = buffer[1];
    current_radar.moving_dist = buffer[2] | (buffer[3] << 8);
    current_radar.moving_energy = buffer[4];
    current_radar.static_dist = buffer[5] | (buffer[6] << 8);
    current_radar.static_energy = buffer[7];
    current_radar.detect_dist = buffer[8] | (buffer[9] << 8);
}

void setup() {
    Serial.begin(115200);
#ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(2000); 
#endif
    Serial.println("\n[RECONFIG] Robust Radar Baud Rate Downgrade Utility");

    // Start at 256000
    RADAR_SERIAL.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
    delay(1000);

    Serial.println("[RECONFIG] Sending: Enable Config...");
    uint8_t enable_config[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    RADAR_SERIAL.write(enable_config, sizeof(enable_config));
    delay(200);

    Serial.println("[RECONFIG] Sending: Set Baud 115200...");
    uint8_t set_baud[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA1, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    RADAR_SERIAL.write(set_baud, sizeof(set_baud));
    delay(200);

    Serial.println("[RECONFIG] Sending: SAVE TO FLASH...");
    uint8_t save_config[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA0, 0x00, 0x04, 0x03, 0x02, 0x01};
    RADAR_SERIAL.write(save_config, sizeof(save_config));
    delay(200);

    Serial.println("[RECONFIG] Sending: RESTART...");
    uint8_t restart_cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFD, 0x00, 0x04, 0x03, 0x02, 0x01};
    RADAR_SERIAL.write(restart_cmd, sizeof(restart_cmd));
    delay(1000);

    Serial.println("[RECONFIG] Switching ESP32 to 115200 to verify...");
    RADAR_SERIAL.begin(115200, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);

    // Watch for valid headers for 5 seconds
    unsigned long start = millis();
    bool success = false;
    while (millis() - start < 10000) {
        if (RADAR_SERIAL.available() >= 4) {
            uint8_t b = RADAR_SERIAL.read();
            if (b == 0xF4) {
                if (RADAR_SERIAL.read() == 0xF3 && RADAR_SERIAL.read() == 0xF2 && RADAR_SERIAL.read() == 0xF1) {
                    success = true;
                    break;
                }
            }
        }
    }

    if (success) {
        Serial.println("\n[SUCCESS] Radar is now talking at 115200 baud!");
    } else {
        Serial.println("\n[FAILED] Still no valid 115200 packets. Try swapping TX/RX wires.");
    }

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

    // Register presence endpoint
    node->register_op("sensor/presence", [](const fs::json& params, fs::VFSResponseWriter *response) {
        fs::json data = {
            {"state", current_radar.target_state},
            {"moving_cm", current_radar.moving_dist},
            {"moving_energy", current_radar.moving_energy},
            {"static_cm", current_radar.static_dist},
            {"static_energy", current_radar.static_energy},
            {"distance_cm", current_radar.detect_dist}
        };
        response->send(200, "application/json", data.dump().c_str());
    }, {{"output", "object"}});

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    // Parse LD2410 UART State Machine
    static uint8_t rx_buffer[64];
    static int rx_idx = 0;
    static bool frame_started = false;
    
    while (RADAR_SERIAL.available()) {
        uint8_t b = RADAR_SERIAL.read();
        rx_buffer[rx_idx++] = b;
        
        if (!frame_started && rx_idx >= 4) {
            if (rx_buffer[rx_idx-4] == 0xF4 && rx_buffer[rx_idx-3] == 0xF3 && 
                rx_buffer[rx_idx-2] == 0xF2 && rx_buffer[rx_idx-1] == 0xF1) {
                rx_buffer[0] = 0xF4; rx_buffer[1] = 0xF3; rx_buffer[2] = 0xF2; rx_buffer[3] = 0xF1;
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
                    static unsigned long last_broadcast = 0;
                    
                    if (current_radar.target_state != last_state || (millis() - last_broadcast > 5000)) {
                        last_state = current_radar.target_state;
                        last_broadcast = millis();
                        
                        fs::Selector selector("sensor/presence");
                        fs::json payload = {
                            {"state", current_radar.target_state},
                            {"distance_cm", current_radar.detect_dist}
                        };
                        node->notify(selector, payload);
                        Serial.printf("[RADAR] State: %d, Dist: %d cm\n", current_radar.target_state, current_radar.detect_dist);
                    }
                }
                
                rx_idx = 0;
                frame_started = false;
            }
        }
        
        if (rx_idx >= 64) {
            rx_idx = 0;
            frame_started = false;
        }
    }
}
