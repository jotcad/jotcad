#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "vfs.h"
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
fs::VFS *node = nullptr;

static float last_temp = 0.0;
static float last_hum = 0.0;
static bool has_data = false;

void refresh_display() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    if (has_data) {
        display.printf("Temp: %.1f C\n", last_temp);
        display.printf("Hum:  %.1f %%\n", last_hum);
    } else if (node && node->is_connected()) {
        display.println("Mesh: Connected");
        display.println("Sensor: Waiting...");
    } else {
        display.println("Mesh: Searching...");
        display.printf("IP: %s", WiFi.localIP().toString().c_str());
    }

    // Status Bar (Bottom)
    display.drawLine(0, SCREEN_HEIGHT - 9, 128, SCREEN_HEIGHT - 9, SSD1306_WHITE);
    display.setCursor(0, SCREEN_HEIGHT - 7);
    
    std::string node_id = node ? node->id() : "unknown";
    if (node_id.length() > 6) {
        node_id = node_id.substr(node_id.length() - 6);
    }
    display.printf("ID: %s", node_id.c_str());
    
    // Activity Blinker
    static bool blink = false;
    if (blink) display.drawPixel(127, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    blink = !blink;

    display.display();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266] SSD1306 OLED Monitor Booting...");

    // Initializing I2C with standard D1/D2 pins
    Wire.begin(D2, D1);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("JotCAD Monitor");
    display.println("Connecting WiFi...");
    display.display();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("WiFi OK");
    display.println("IP: " + WiFi.localIP().toString());
    display.display();
    delay(1000);

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    fs::VFS::Config config;
    char unique_id[64];
    uint32_t chipId = ESP.getChipId();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(chipId & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    
    node = new fs::VFS(config);

    // 1. React to Temperature Updates
    node->on_notification("sensor/temperature", [](const fs::Selector& sel, const fs::json& payload) {
        if (payload.contains("value")) {
            last_temp = payload["value"].get<float>();
            has_data = true;
            refresh_display();
            Serial.printf("[OLED] Temp Update: %.1f\n", last_temp);
        }
    });

    // 2. React to Humidity Updates
    node->on_notification("sensor/humidity", [](const fs::Selector& sel, const fs::json& payload) {
        if (payload.contains("value")) {
            last_hum = payload["value"].get<float>();
            has_data = true;
            refresh_display();
            Serial.printf("[OLED] Hum Update: %.1f\n", last_hum);
        }
    });

    node->begin();

    // 3. Declare Interest
    node->subscribe(fs::Selector("sensor/temperature"), 0);
    node->subscribe(fs::Selector("sensor/humidity"), 0);

    Serial.println("VFS_READY");
}

void loop() {
    node->tick();
    
    static unsigned long last_refresh = 0;
    if (millis() - last_refresh > 1000) {
        last_refresh = millis();
        refresh_display();
    }

    static unsigned long last_sub = 0;
    if (millis() - last_sub > 30000) {
        last_sub = millis();
        node->subscribe(fs::Selector("sensor/temperature"), 0);
        node->subscribe(fs::Selector("sensor/humidity"), 0);
        node->log_status();
    }
}
