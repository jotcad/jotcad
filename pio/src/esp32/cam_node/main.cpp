#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "vfs.h"
#include "secrets.h"

// AI-Thinker ESP32-CAM Pin Map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

fs::VFS *node = nullptr;

bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Use lower resolution for faster transmission over VFS initially
    if(psramFound()){
        config.frame_size = FRAMESIZE_QVGA; // 320x240
        config.jpeg_quality = 12;
        config.fb_count = 1;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 15;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return false;
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32-CAM] JotCAD VFS Node Booting...");

    if (!init_camera()) {
        Serial.println("Camera Init FAILED. System will continue as metadata-only node.");
    }

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
static_assert(sizeof(DEVICE_NODE_ID) > 2, "DEVICE_NODE_ID cannot be empty! Please define sysenv.DEVICE_NODE_ID in your environment.");

    // 1. Setup VFS Node
    fs::VFS::Config config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};

    node = new fs::VFS(config);



    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();

    static unsigned long last_capture = 0;

    // Auto-publish every 500ms (2 FPS) if someone is interested
    if (millis() - last_capture > 500) {
        last_capture = millis();

        camera_fb_t * fb = esp_camera_fb_get();
        if (fb) {
            node->publish_binary("sensor/camera", fb->buf, fb->len);
            esp_camera_fb_return(fb);
        }
    }
}
