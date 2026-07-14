#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "vfs.h"
#include "secrets.h"

// Stepper pin configurations (D1, D2, D5, D6)
#ifndef IN1_PIN
#define IN1_PIN D1
#endif
#ifndef IN2_PIN
#define IN2_PIN D2
#endif
#ifndef IN3_PIN
#define IN3_PIN D5
#endif
#ifndef IN4_PIN
#define IN4_PIN D6
#endif

fs::VFS *node = nullptr;
bool was_connected = false;

// 28BYJ-48 Half-step sequence (8 states)
const int step_sequence[8][4] = {
    {1, 0, 0, 0}, // State 0
    {1, 1, 0, 0}, // State 1
    {0, 1, 0, 0}, // State 2
    {0, 1, 1, 0}, // State 3
    {0, 0, 1, 0}, // State 4
    {0, 0, 1, 1}, // State 5
    {0, 0, 0, 1}, // State 6
    {1, 0, 0, 1}  // State 7
};

// State variables
int current_step_state = 0;
long current_position_steps = 0;
long target_position_steps = 0;
long start_position_steps = 0;
unsigned long step_delay_us = 1200; // Requested target delay between steps
unsigned long current_step_delay_us = 1200; // Instantaneous delay during ramping
unsigned long last_step_time_us = 0;
bool is_moving = false;

// Kinematics reports updated dynamically in loop()
double current_velocity_steps_sec = 0.0;
double current_velocity_deg_sec = 0.0;
double current_acceleration_steps_sec2 = 0.0;
double current_acceleration_deg_sec2 = 0.0;

void write_outputs(int state_index) {
    digitalWrite(IN1_PIN, step_sequence[state_index][0]);
    digitalWrite(IN2_PIN, step_sequence[state_index][1]);
    digitalWrite(IN3_PIN, step_sequence[state_index][2]);
    digitalWrite(IN4_PIN, step_sequence[state_index][3]);
}

void release_motor() {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    digitalWrite(IN3_PIN, LOW);
    digitalWrite(IN4_PIN, LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP8266-Stepper] Stepper Control Node Booting...");

    // Setup hardware pins
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);
    release_motor(); // De-energize coils to prevent heating at startup

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[ESP8266-Stepper] Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[ESP8266-Stepper] WiFi Connected.");
    Serial.printf("[ESP8266-Stepper] IP Address: %s\n", WiFi.localIP().toString().c_str());

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

    // Register queryable for stepper status
    node->register_op("actuator/stepper/state", [](const fs::json&, fs::VFSResponseWriter* response) {
        double current_angle = (current_position_steps % 4096) * 360.0 / 4096.0;
        if (current_angle < 0) current_angle += 360.0;

        fs::json state = {
            {"position_steps", current_position_steps},
            {"angle_degrees", current_angle},
            {"velocity_steps_sec", current_velocity_steps_sec},
            {"velocity_deg_sec", current_velocity_deg_sec},
            {"acceleration_steps_sec2", current_acceleration_steps_sec2},
            {"acceleration_deg_sec2", current_acceleration_deg_sec2}
        };
        response->send(200, "application/json", state.dump().c_str());
    });

    // Subscribe to targets: actuator/stepper/target
    node->subscribe("actuator/stepper/target", [](const fs::json& payload) {
        Serial.println("[ESP8266-Stepper] Received new target payload!");

        // 1. Resolve target position
        if (payload.contains("steps")) {
            long steps = payload["steps"];
            bool relative = payload.value("relative", true);
            if (relative) {
                target_position_steps = current_position_steps + steps;
            } else {
                target_position_steps = steps;
            }
        } else if (payload.contains("angle")) {
            double angle = payload["angle"];
            bool relative = payload.value("relative", true);
            long delta_steps = (angle / 360.0) * 4096.0;
            if (relative) {
                target_position_steps = current_position_steps + delta_steps;
            } else {
                target_position_steps = delta_steps;
            }
        }

        // 2. Resolve speed limit
        if (payload.contains("speed")) {
            unsigned long delay_val = payload["speed"];
            // 28BYJ-48 limits: min delay ~900us for half-step stability
            step_delay_us = max(delay_val, (unsigned long)900);
        } else {
            step_delay_us = 1200; // Default speed
        }

        start_position_steps = current_position_steps;
        is_moving = (target_position_steps != current_position_steps);
        Serial.printf("[ESP8266-Stepper] New Target -> Current: %ld, Target: %ld, Step Delay: %lu us\n",
                      current_position_steps, target_position_steps, step_delay_us);
    });

    if (node->is_connected()) {
        was_connected = true;
        Serial.println("[ESP8266-Stepper] VFS Node Ready.");
        Serial.println("VFS_READY");
    }
}

void loop() {
    node->tick();

    // Check VFS connection state changes
    bool current_connected = node->is_connected();
    if (current_connected != was_connected) {
        was_connected = current_connected;
        if (current_connected) {
            Serial.println("[ESP8266-Stepper] VFS Reconnected.");
            Serial.println("VFS_READY");
        } else {
            Serial.println("[ESP8266-Stepper] VFS Connection Lost.");
        }
    }

    // Stepper movement state machine (Non-blocking)
    if (is_moving) {
        long total_steps = abs(target_position_steps - start_position_steps);
        long steps_moved = abs(current_position_steps - start_position_steps);
        long steps_remaining = abs(target_position_steps - current_position_steps);

        long ramp_steps = min((long)100, total_steps / 2);
        current_step_delay_us = step_delay_us;
        unsigned long start_delay = 2500; // Starting delay (slower speed)

        int step_direction = (target_position_steps > current_position_steps) ? 1 : -1;

        // Calculate instantaneous ramping delay and average acceleration
        if (steps_moved < ramp_steps && ramp_steps > 0) {
            // Accelerating: decrease delay from start_delay to step_delay_us
            current_step_delay_us = start_delay - ((start_delay - step_delay_us) * steps_moved / ramp_steps);
            
            double v_start = 1000000.0 / start_delay;
            double v_target = 1000000.0 / step_delay_us;
            current_acceleration_steps_sec2 = step_direction * (v_target * v_target - v_start * v_start) / (2.0 * ramp_steps);
        } else if (steps_remaining < ramp_steps && ramp_steps > 0) {
            // Decelerating: increase delay from step_delay_us to start_delay
            current_step_delay_us = step_delay_us + ((start_delay - step_delay_us) * (ramp_steps - steps_remaining) / ramp_steps);
            
            double v_start = 1000000.0 / start_delay;
            double v_target = 1000000.0 / step_delay_us;
            current_acceleration_steps_sec2 = -step_direction * (v_target * v_target - v_start * v_start) / (2.0 * ramp_steps);
        } else {
            // Cruising at target speed
            current_step_delay_us = step_delay_us;
            current_acceleration_steps_sec2 = 0.0;
        }

        // Calculate velocity
        current_velocity_steps_sec = (step_direction * 1000000.0) / current_step_delay_us;
        current_velocity_deg_sec = current_velocity_steps_sec * (360.0 / 4096.0);
        current_acceleration_deg_sec2 = current_acceleration_steps_sec2 * (360.0 / 4096.0);

        unsigned long now_us = micros();
        if (now_us - last_step_time_us >= current_step_delay_us) {
            last_step_time_us = now_us;

            // Increment/Decrement state step (0 to 7)
            current_step_state = (current_step_state + step_direction) & 7;
            write_outputs(current_step_state);

            current_position_steps += step_direction;

            if (current_position_steps == target_position_steps) {
                is_moving = false;
                current_step_delay_us = step_delay_us; // Reset to target delay when stationary
                current_velocity_steps_sec = 0.0;
                current_velocity_deg_sec = 0.0;
                current_acceleration_steps_sec2 = 0.0;
                current_acceleration_deg_sec2 = 0.0;
                release_motor(); // Release coils to prevent motor overheating
                Serial.printf("[ESP8266-Stepper] Target Reached! Position: %ld steps\n", current_position_steps);

                // Publish absolute state update to VFS mesh
                double current_angle = (current_position_steps % 4096) * 360.0 / 4096.0;
                if (current_angle < 0) current_angle += 360.0;
                fs::json update = {
                    {"position_steps", current_position_steps},
                    {"angle_degrees", current_angle},
                    {"velocity_steps_sec", 0.0},
                    {"velocity_deg_sec", 0.0},
                    {"acceleration_steps_sec2", 0.0},
                    {"acceleration_deg_sec2", 0.0}
                };
                node->publish("actuator/stepper/state", update);
            }
        }
    }
}
