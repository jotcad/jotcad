#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

// Pin configurations (provided by platformio.ini macros)
#ifndef PIN_JOYSTICK_X
#define PIN_JOYSTICK_X 3
#endif
#ifndef PIN_JOYSTICK_Y
#define PIN_JOYSTICK_Y 5
#endif
#ifndef PIN_JOYSTICK_SW
#define PIN_JOYSTICK_SW 7
#endif

fs::VFS *node = nullptr;

// Filter thresholds
const int ADC_THRESHOLD = 32;       // Only publish if X/Y change by more than this to reduce network spam
const unsigned long HEARTBEAT_MS = 1000; // Heartbeat interval to force publish state
const unsigned long READ_INTERVAL_MS = 20; // Sample physical inputs every 20ms

// Last published states
int last_x = -1;
int last_y = -1;
bool last_pressed = false;
unsigned long last_publish_time = 0;
unsigned long last_read_time = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32-Joystick] Joystick VFS Node Booting...");

    // Setup joystick hardware
    // GPIO 34 & 35 are ADC1 channels. GPIO 32 is standard GPIO.
    pinMode(PIN_JOYSTICK_X, INPUT);
    pinMode(PIN_JOYSTICK_Y, INPUT);
    pinMode(PIN_JOYSTICK_SW, INPUT_PULLUP);

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[ESP32-Joystick] Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.printf("[ESP32-Joystick] IP Address: %s\n", WiFi.localIP().toString().c_str());

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // Setup VFS configuration
    fs::VFS::Config config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};

    delay(2000); // Wait for router network stack to stabilize

    // Optional: Probe host gateway health
    Serial.println("[ESP32-Joystick] Probing host gateway health (GET /health)...");
    HTTPClient probe;
    probe.begin(String(MESH_NEIGHBOR_URL) + "/health");
    int probeCode = probe.GET();
    if (probeCode > 0) {
        Serial.printf("[ESP32-Joystick] Gateway sanity OK: %d\n", probeCode);
    } else {
        Serial.printf("[ESP32-Joystick] Gateway sanity FAILED: %s (%d)\n", probe.errorToString(probeCode).c_str(), probeCode);
    }
    probe.end();

    // Start VFS node
    node = new fs::VFS(config);

    // Register W3C Thing Description endpoint to serve schema dynamically
    node->register_op("sensor/joystick/td", [](const fs::json&, fs::VFSResponseWriter* response) {
        std::string td_json = R"({
  "@context": [
    "https://www.w3.org/2019/wot/td/v1",
    {
      "jot": "https://jotcad.org/ns#"
    }
  ],
  "id": "urn:dev:ops:esp32-joystick-01",
  "title": "ESP32 Joystick Node",
  "description": "Dual-axis analog joystick with press button integrated via Zenoh VFS",
  "securityDefinitions": {
    "nosec_sc": {
      "scheme": "nosec"
    }
  },
  "security": [
    "nosec_sc"
  ],
  "properties": {
    "coordinates": {
      "title": "Joystick Coordinates",
      "description": "Raw 12-bit X and Y coordinates",
      "type": "object",
      "properties": {
        "x": { "type": "integer", "minimum": 0, "maximum": 4095 },
        "y": { "type": "integer", "minimum": 0, "maximum": 4095 }
      },
      "jot:visualizer": "joystick-grid-2d",
      "forms": [
        {
          "href": "ws://127.0.0.1:10000",
          "jot:topic": "sensor/joystick",
          "contentType": "application/json",
          "op": ["observeproperty"]
        }
      ]
    },
    "pressed": {
      "title": "Button State",
      "description": "Digital switch button state",
      "type": "boolean",
      "jot:visualizer": "status-label",
      "forms": [
        {
          "href": "ws://127.0.0.1:10000",
          "jot:topic": "sensor/joystick",
          "contentType": "application/json",
          "op": ["observeproperty"]
        }
      ]
    }
  }
})";
        response->send(200, "application/json", td_json.c_str());
    });

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    unsigned long now = millis();
    if (now - last_read_time >= READ_INTERVAL_MS) {
        last_read_time = now;

        // Read analog inputs (ESP32 ADC has 12-bit resolution: 0 to 4095)
        int raw_x = analogRead(PIN_JOYSTICK_X);
        int raw_y = analogRead(PIN_JOYSTICK_Y);
        
        // Joystick switch is active low (pressed == LOW)
        bool pressed = (digitalRead(PIN_JOYSTICK_SW) == LOW);

        // Detect if changes exceed thresholds or switch state toggled
        bool x_changed = abs(raw_x - last_x) > ADC_THRESHOLD;
        bool y_changed = abs(raw_y - last_y) > ADC_THRESHOLD;
        bool press_changed = (pressed != last_pressed);
        bool should_publish = x_changed || y_changed || press_changed || (now - last_publish_time >= HEARTBEAT_MS);

        if (should_publish && node->is_connected()) {
            last_x = raw_x;
            last_y = raw_y;
            last_pressed = pressed;
            last_publish_time = now;

            // Build payload
            fs::json payload = {
                {"x", raw_x},
                {"y", raw_y},
                {"pressed", pressed}
            };

            // Publish coordinates to the VFS cluster topic
            node->publish("sensor/joystick", payload);

            Serial.printf("[ESP32-Joystick] Published state -> X: %d, Y: %d, SW: %s\n", 
                          raw_x, raw_y, pressed ? "PRESSED" : "RELEASED");
        }
    }
}
