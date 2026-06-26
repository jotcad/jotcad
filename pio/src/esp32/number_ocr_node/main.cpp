#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "vfs.h"
#include "secrets.h"

// TensorFlow Lite Micro Headers
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Embedded Neural Network Model Header
#include "model_data.h"

// AI-Thinker ESP32-CAM Pin Configuration
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

// TensorFlow Lite Micro Globals
namespace {
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;

    // 128KB tensor arena allocated in fast Internal DRAM
    constexpr int kTensorArenaSize = 128 * 1024;
    uint8_t* tensor_arena = nullptr;
}

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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000;
    
    // Configured for Grayscale 160x120 pixels (Natively supported QQVGA)
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    // Correct the 180-degree physical mounting rotation in hardware
    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        s->set_hmirror(s, 0); // Disable horizontal mirror (correct left-right mirroring)
        s->set_vflip(s, 1);   // Flip vertically (correct top-bottom mirroring)
    }

    return true;
}

bool init_tflite() {
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    Serial.printf("[OCR] Internal Free Heap: %d, PSRAM Free: %d\n", 
                  ESP.getFreeHeap(), ESP.getFreePsram());

    Serial.print("[OCR] Model data header in Flash: ");
    for (int i = 0; i < 40; i++) {
        Serial.printf("%02x ", model_data[i]);
    }
    Serial.println();

    // Allocate 128KB Arena in DRAM (Internal SRAM) for low-latency atomic access
    tensor_arena = (uint8_t*)malloc(kTensorArenaSize);
    if (tensor_arena) {
        Serial.println("[OCR] Allocated 128KB Arena in Internal SRAM (DRAM)");
    } else {
        Serial.println("[OCR] FAILED to allocate tensor arena in SRAM!");
        return false;
    }

    // Copy model flatbuffer to RAM to prevent misaligned Flash reading anomalies
    uint8_t* ram_model_data = nullptr;
    if (psramFound()) {
        ram_model_data = (uint8_t*)ps_malloc(model_data_len);
        if (ram_model_data) {
            memcpy(ram_model_data, model_data, model_data_len);
            Serial.println("[OCR] Copied model data to PSRAM for safe aligned execution");
        }
    }
    
    if (!ram_model_data) {
        ram_model_data = (uint8_t*)malloc(model_data_len);
        if (ram_model_data) {
            memcpy(ram_model_data, model_data, model_data_len);
            Serial.println("[OCR] Copied model data to Internal DRAM for safe aligned execution");
        }
    }

    if (!ram_model_data) {
        Serial.println("[OCR] FAILED to allocate memory for model copy!");
        return false;
    }

    // Load the model flatbuffer from the safe RAM copy
    model = tflite::GetModel(ram_model_data);
    
    // Safety: Verify model signature and version
    if (model->version() != 3) {
        Serial.printf("[OCR] WARNING: Model version %d != 3. Data might be corrupt or misaligned.\n", (int)model->version());
    }

    // Load operations resolver
    static tflite::AllOpsResolver resolver;

    // Instantiate interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        Serial.printf("[OCR] AllocateTensors() failed with status %d\n", allocate_status);
        return false;
    }

    // Set input and output tensor pointers
    input = interpreter->input(0);
    output = interpreter->output(0);
    if (!input || !output) {
        Serial.println("[OCR] Failed to retrieve input/output tensors!");
        return false;
    }

    Serial.printf("[OCR] Input type: %d, Output type: %d (kTfLiteFloat32=1, kTfLiteInt8=9)\n", 
                  input->type, output->type);

    return true;
}

inline float get_bilinear_pixel(const uint8_t* frame, int src_w, int src_h, float x, float y) {
    int x1 = (int)x;
    int y1 = (int)y;
    int x2 = x1 + 1;
    int y2 = y1 + 1;
    
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= src_w) x2 = src_w - 1;
    if (y2 >= src_h) y2 = src_h - 1;
    
    float dx = x - x1;
    float dy = y - y1;
    
    float p11 = frame[y1 * src_w + x1];
    float p21 = frame[y1 * src_w + x2];
    float p12 = frame[y2 * src_w + x1];
    float p22 = frame[y2 * src_w + x2];
    
    return p11 * (1 - dx) * (1 - dy) +
           p21 * dx * (1 - dy) +
           p12 * (1 - dx) * dy +
           p22 * dx * dy;
}

// Preprocesses a cropped grayscale image segment: finds min/max, determines polarity,
// detects active digit bounding box, adds padding, resizes it to 48x48 using bilinear interpolation,
// and normalizes pixel values to [0.0, 1.0] with a soft-threshold.
// Returns true if the image is light-on-dark (LED), false otherwise.
bool preprocess_digit(const uint8_t* frame, int src_w, int src_h, int crop_x, int crop_y, int crop_w, int crop_h, float* dst_tensor) {
    // 1. Estimate background level from the outer 6-pixel border
    long bg_total = 0;
    long bg_count = 0;
    for (int y = 0; y < crop_h; y++) {
        for (int x = 0; x < crop_w; x++) {
            if (x < 6 || x >= crop_w - 6 || y < 6 || y >= crop_h - 6) {
                bg_total += frame[(crop_y + y) * src_w + (crop_x + x)];
                bg_count++;
            }
        }
    }
    uint8_t bg_avg = bg_total / bg_count;

    // 2. Measure min/max in the central region (excluding outer 8 pixels) to find the contrast range
    uint8_t min_val = 255;
    uint8_t max_val = 0;
    for (int y = 8; y < crop_h - 8; y++) {
        for (int x = 8; x < crop_w - 8; x++) {
            uint8_t val = frame[(crop_y + y) * src_w + (crop_x + x)];
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
    }
    float range = max_val - min_val;

    // Determine polarity
    bool light_on_dark = (bg_avg < 128); // Background is dark -> Light-on-Dark

    // If contrast range is too low, clear tensor and return
    if (range < 25) {
        for (int i = 0; i < 48 * 48; i++) dst_tensor[i] = 0.0f;
        return light_on_dark;
    }

    // 3. Detect active bounding box in central region (excluding outer 8 pixels)
    int min_x = crop_w, max_x = -1;
    int min_y = crop_h, max_y = -1;
    
    for (int y = 8; y < crop_h - 8; y++) {
        for (int x = 8; x < crop_w - 8; x++) {
            uint8_t val = frame[(crop_y + y) * src_w + (crop_x + x)];
            bool is_active = false;
            if (light_on_dark) {
                is_active = (val > bg_avg + 35);
            } else {
                is_active = (val < bg_avg - 35);
            }
            if (is_active) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    // If no active region is found or it's tiny (noise), clear input and return
    if (max_x == -1 || (max_x - min_x + 1) < 6 || (max_y - min_y + 1) < 8) {
        for (int i = 0; i < 48 * 48; i++) dst_tensor[i] = 0.0f;
        return light_on_dark;
    }

    int bbox_w = max_x - min_x + 1;
    int bbox_h = max_y - min_y + 1;
    
    // Add 12% padding to each side of the digit bounding box
    int pad_x = bbox_w * 0.12f;
    int pad_y = bbox_h * 0.12f;
    if (pad_x < 2) pad_x = 2;
    if (pad_y < 2) pad_y = 2;
    
    int x1 = min_x - pad_x;
    int y1 = min_y - pad_y;
    int x2 = max_x + pad_x;
    int y2 = max_y + pad_y;
    
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= crop_w) x2 = crop_w - 1;
    if (y2 >= crop_h) y2 = crop_h - 1;
    
    int final_w = x2 - x1 + 1;
    int final_h = y2 - y1 + 1;

    // Rescale bounding box to 48x48 using bilinear interpolation
    for (int y = 0; y < 48; y++) {
        float frame_y = crop_y + y1 + (y * final_h) / 48.0f;
        for (int x = 0; x < 48; x++) {
            float frame_x = crop_x + x1 + (x * final_w) / 48.0f;
            float pixel = get_bilinear_pixel(frame, src_w, src_h, frame_x, frame_y);
            
            float val_norm = 0.0f;
            if (light_on_dark) {
                float low = bg_avg + 15.0f;
                float high = bg_avg + 55.0f;
                if (high > max_val) high = max_val;
                if (high <= low) high = low + 5.0f;

                if (pixel <= low) {
                    val_norm = 0.0f;
                } else if (pixel >= high) {
                    val_norm = 1.0f;
                } else {
                    val_norm = (pixel - low) / (high - low);
                }
            } else {
                float low = bg_avg - 15.0f;
                float high = bg_avg - 55.0f;
                if (high < min_val) high = min_val;
                if (high >= low) high = low - 5.0f;

                if (pixel >= low) {
                    val_norm = 0.0f;
                } else if (pixel <= high) {
                    val_norm = 1.0f;
                } else {
                    val_norm = (low - pixel) / (low - high);
                }
            }
            dst_tensor[y * 48 + x] = val_norm;
        }
    }

    return light_on_dark;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32-CAM] Single-Digit OCR VFS Node Booting...");

    if (!init_camera()) {
        Serial.println("Camera Init FAILED.");
    }
    if (!init_tflite()) {
        Serial.println("TensorFlow Lite Micro Init FAILED.");
    } else {
        Serial.println("On-device TensorFlow Lite Micro loaded successfully!");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");

#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined!"
#endif

    // Setup VFS Mesh Connection
    fs::VFS::Config vfs_config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    vfs_config.id = unique_id;
    vfs_config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    vfs_config.neighbors = {MESH_NEIGHBOR_URL};
    
    node = new fs::VFS(vfs_config);

    // Formally advertise sensor/camera capability to the mesh routing engine
    

    node->begin();
    Serial.println("VFS_READY");
}

void loop() {
    node->tick();
    
    static unsigned long last_inference = 0;
    
    // Run OCR every 2.0 seconds to conserve CPU while keeping mesh responsive
    if (millis() - last_inference > 2000) {
        last_inference = millis();
        
        if (!interpreter || !input) {
            // Guard: TensorFlow Lite Micro not loaded successfully
            return;
        }
        
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[OCR] Failed to capture camera frame");
            return;
        }

        // Center 96x96 pixels crop within native 160x120 QQVGA frame
        const int cropSize = 96;
        const int startX = (160 - cropSize) / 2; // = 32
        const int startY = (120 - cropSize) / 2; // = 12

        // Publish cropped diagnostic frame with dimensions and format in selector parameters as binary
        std::vector<uint8_t> cropped_bytes(cropSize * cropSize);
        for (int y = 0; y < cropSize; y++) {
            int src_y = startY + y;
            for (int x = 0; x < cropSize; x++) {
                int src_x = startX + x;
                cropped_bytes[y * cropSize + x] = fb->buf[src_y * 160 + src_x];
            }
        }

        fs::json cam_params = {
            {"width", cropSize},
            {"height", cropSize},
            {"format", "grayscale"}
        };
        fs::Selector cam_selector("sensor/camera", cam_params);

        int cam_sent = node->notify_binary(cam_selector, cropped_bytes.data(), cropped_bytes.size());
        if (cam_sent > 0) {
            Serial.printf("[OCR Node] Published Cropped Binary Frame (%d bytes, Sent to %d interested peers)\n", 
                          (int)cropped_bytes.size(), cam_sent);
        } else {
            Serial.printf("[OCR Node] Cropped Binary Frame Capture complete (0 interested peers, pub skipped)\n");
        }

        // 2. Preprocess (crop 96x96, bounding-box pad, bilinear resize to 48x48, min-max normalize) and populate input tensor
        bool light_on_dark = preprocess_digit(fb->buf, 160, 120, startX, startY, cropSize, cropSize, input->data.f);

        // 3. Publish the 48x48 preprocessed frame to VFS for visualization and diagnostics
        std::vector<uint8_t> prep_bytes(48 * 48);
        for (int i = 0; i < 48 * 48; i++) {
            prep_bytes[i] = (uint8_t)(input->data.f[i] * 255.0f);
        }

        fs::json prep_params = {
            {"width", 48},
            {"height", 48},
            {"format", "grayscale"}
        };
        fs::Selector prep_selector("sensor/camera/preprocessed", prep_params);

        int prep_sent = node->notify_binary(prep_selector, prep_bytes.data(), prep_bytes.size());
        if (prep_sent > 0) {
            Serial.printf("[OCR Node] Published Preprocessed Frame (%d bytes, Sent to %d interested peers)\n", 
                          (int)prep_bytes.size(), prep_sent);
        }

        // Run TFLite Micro inference
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            Serial.println("[TFLite] Inference invocation failed");
            esp_camera_fb_return(fb);
            return;
        }

        // Return camera frame buffer
        esp_camera_fb_return(fb);

        // Read outputs from the 11-class classification head
        int max_index = 0;
        float max_score = -999.0f;
        float digit_confidence = 0.0f;

        if (output->type == kTfLiteInt8) {
            int8_t* active_scores = output->data.int8;
            int8_t max_int8_score = -128;
            for (int c = 0; c < 11; c++) {
                int8_t score = active_scores[c];
                if (score > max_int8_score) {
                    max_int8_score = score;
                    max_index = c;
                }
            }
            digit_confidence = (float)(max_int8_score + 128) / 255.0f;
        } else {
            float* active_scores = output->data.f;
            float max_float_score = -1.0f;
            for (int c = 0; c < 11; c++) {
                float score = active_scores[c];
                if (score > max_float_score) {
                    max_float_score = score;
                    max_index = c;
                }
            }
            digit_confidence = max_float_score;
        }

        fs::json selector = {{"path", "sensor/vision/digits"}};
        fs::json payload;

        if (max_index == 10) {
            // Predicted No-Digit (class 10)
            Serial.printf("[OCR Node] Live Inference: No digit detected (Confidence: %d%%) [%s]\n", 
                          (int)(digit_confidence * 100),
                          light_on_dark ? "Light-on-Dark" : "Dark-on-Light");
            
            payload = {
                {"number", nullptr},
                {"digits", ""},
                {"confidence", digit_confidence}
            };
        } else {
            String detected_number = String(max_index);
            Serial.printf("[OCR Node] Live Inference: %s (Confidence: %d%%) [%s]\n", 
                          detected_number.c_str(), (int)(digit_confidence * 100),
                          light_on_dark ? "Light-on-Dark" : "Dark-on-Light");
            
            payload = {
                {"number", max_index},
                {"digits", detected_number.c_str()},
                {"confidence", digit_confidence}
            };
        }

        // Publish OCR result dynamically to VFS subscribers on every frame
        node->publish(selector.path, payload);
        if (sent > 0) {
            Serial.printf("[OCR Node] Published Digit Data (%d bytes, Sent to %d interested peers)\n", 
                          (int)payload.dump().length(), sent);
        } else {
            Serial.printf("[OCR Node] Digit Data published (0 interested peers, pub skipped)\n");
        }
    }
}
