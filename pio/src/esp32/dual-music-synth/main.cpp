#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

// BLE-MIDI and standard BLE scanning imports
#include <BLEMidi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Classic Bluetooth A2DP Source and Synthesizer imports
#include "BluetoothA2DPSource.h"
#include "ESP32Synth.h"

// --- Configuration Options ---
// Set to 1 to enable VFS mesh networking (disabled by default for low-latency A2DP audio streaming)
#define ENABLE_VFS 0

// Target name of your Bluetooth Earphones/Speakers (e.g., "JBL Flip 6", "Sony WH-1000XM4").
// If set to an empty string "", it will automatically connect to the first advertising Bluetooth audio receiver.
#define BT_EARPHONES_NAME ""

// --- Synthesizer Globals ---
ESP32Synth synth;
BluetoothA2DPSource a2dp_source;

// 16-note polyphonic voice allocation mapper
#define MAX_POLYPHONY 16
static int active_notes[MAX_POLYPHONY];
static int next_voice_index = 0;
static portMUX_TYPE synth_mux = portMUX_INITIALIZER_UNLOCKED;

// Standard BLE MIDI Service UUID
static BLEUUID midiServiceUUID("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");

// --- BLE-MIDI Discovery Scan Callbacks ---
class JotMidiDiscoveryCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("[BLE-SCANNER] Discovered device: ");
        Serial.printf("Name: '%s' | Addr: %s | RSSI: %d dBm", 
                      advertisedDevice.getName().c_str(),
                      advertisedDevice.getAddress().toString().c_str(),
                      advertisedDevice.getRSSI());
        
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(midiServiceUUID)) {
            Serial.print("  <== [MIDI-CAPABLE KEYBOARD DETECTED!]");
        }
        Serial.println();
    }
};

// --- Thread-Safe Event Tracker ---
static fs::json last_midi_event = {{"type", "none"}};
static portMUX_TYPE event_mux = portMUX_INITIALIZER_UNLOCKED;

void set_last_event(const fs::json& event) {
    portENTER_CRITICAL(&event_mux);
    last_midi_event = event;
    portEXIT_CRITICAL(&event_mux);
}

fs::json get_last_event() {
    fs::json event;
    portENTER_CRITICAL(&event_mux);
    event = last_midi_event;
    portEXIT_CRITICAL(&event_mux);
    return event;
}

// --- VFS Mesh Event Broadcaster ---
fs::VFS *node = nullptr;

void broadcast_vfs_event(const fs::json& event) {
    set_last_event(event);
#if ENABLE_VFS
    if (new_event) {
        fs::Selector selector("midi/event");
        node->notify(selector, event);
    }

#endif
}

// --- Real-time Stereo Audio Stream Callback (Invoked on Core 0 by A2DP Thread) ---
static uint64_t total_frames_rendered = 0;

int32_t get_sound_data(Frame* data, int32_t len) {
    // Fill the stereo frame buffer in real-time using fixed-point polyphonic mixing
    synth.generateSamplesStereo((int16_t*)data, len);
    total_frames_rendered += len;
    return len;
}

// --- BLE-MIDI Client Event Callbacks ---
void OnBLEConnected() {
    Serial.println("\n======================================================================");
    Serial.println("[BLE-MIDI] >>> SUCCESS: Connected to MIDI Keyboard Controller! <<<");
    Serial.println("======================================================================");
}

void OnBLEDisconnected() {
    Serial.println("\n======================================================================");
    Serial.println("[BLE-MIDI] >>> WARNING: Disconnected from MIDI Keyboard Controller. <<<");
    Serial.println("======================================================================");
}

void OnBLENoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] NOTE ON - Ch: %d, Note: %d, Velocity: %d (Time: %lu ms)\n", 
                  channel, note, velocity, millis());
    
    // Convert MIDI note number to frequency in centihertz
    double freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
    uint32_t freqCentiHz = (uint32_t)(freq * 100.0);

    portENTER_CRITICAL(&synth_mux);
    // Allocate a voice index using round-robin polyphonic allocation
    int voice = next_voice_index;
    next_voice_index = (next_voice_index + 1) % MAX_POLYPHONY;
    active_notes[voice] = note;
    
    // Scale standard 7-bit velocity (0-127) to synthesizer standard 8-bit volume (0-255)
    uint8_t synthVolume = (velocity > 127) ? 255 : (velocity << 1); 
    
    synth.noteOn(voice, freqCentiHz, synthVolume);
    portEXIT_CRITICAL(&synth_mux);

    fs::json event = {
        {"type", "note_on"},
        {"channel", channel},
        {"note", note},
        {"velocity", velocity},
        {"source", "ble"},
        {"timestamp", millis()}
    };
    broadcast_vfs_event(event);
}

void OnBLENoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] NOTE OFF - Ch: %d, Note: %d, Velocity: %d (Time: %lu ms)\n", 
                  channel, note, velocity, millis());

    portENTER_CRITICAL(&synth_mux);
    // Find the active voice channel rendering this note and trigger release envelope
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        if (active_notes[i] == note) {
            synth.noteOff(i);
            active_notes[i] = -1; // De-allocate voice
        }
    }
    portEXIT_CRITICAL(&synth_mux);

    fs::json event = {
        {"type", "note_off"},
        {"channel", channel},
        {"note", note},
        {"velocity", velocity},
        {"source", "ble"},
        {"timestamp", millis()}
    };
    broadcast_vfs_event(event);
}

void OnBLEControlChange(uint8_t channel, uint8_t control, uint8_t value, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] CONTROL CHANGE - Ch: %d, CC: %d, Value: %d (Timestamp: %u)\n", 
                  channel, control, value, timestamp);
    
    fs::json event = {
        {"type", "control_change"},
        {"channel", channel},
        {"control", control},
        {"value", value},
        {"source", "ble"},
        {"timestamp", timestamp}
    };
    broadcast_vfs_event(event);
}

// --- Classic Bluetooth A2DP Device Discovery Callback ---
bool my_ssid_callback(const char* ssid, esp_bd_addr_t address, int rssi) {
    Serial.printf("[A2DP-SCANNER] Discovered device: Name: '%s' | MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm\n",
                  (ssid && strlen(ssid) > 0 ? ssid : "<Unknown>"),
                  address[0], address[1], address[2], address[3], address[4], address[5],
                  rssi);
                  
    // If we have a specific earphones target configured, check if this matches
    if (strlen(BT_EARPHONES_NAME) > 0) {
        if (ssid && strcmp(ssid, BT_EARPHONES_NAME) == 0) {
            Serial.printf("[A2DP-SCANNER] ==> MATCH FOUND: Connecting to locked target '%s'!\n", BT_EARPHONES_NAME);
            return true;
        }
        return false; // Continue scanning for the matching name
    }
    
    // Otherwise, in generic discovery mode, automatically connect to the first audio receiver found
    Serial.printf("[A2DP-SCANNER] ==> Connecting to first available receiver: '%s'...\n", (ssid && strlen(ssid) > 0 ? ssid : "<Unknown>"));
    return true;
}

// --- A2DP State Changed Callbacks for Handshake Diagnostics ---
void on_a2dp_connection_state(esp_a2d_connection_state_t state, void* obj) {
    Serial.printf("[A2DP-CALLBACK] Connection State Changed: %d\n", (int)state);
}

void on_a2dp_audio_state(esp_a2d_audio_state_t state, void* obj) {
    Serial.printf("[A2DP-CALLBACK] Audio State Changed: %d\n", (int)state);
}

// --- Active Bluetooth A2DP Source Connection Verification ---
void verify_a2dp_connection() {
    if (a2dp_source.is_connected()) {
        Serial.println("[A2DP-AUDIO] >>> SUCCESS: Connected to Bluetooth Earphones! Streaming audio... <<<");
        a2dp_source.set_volume(100); // Unmute and set volume to 100/127 on connection!
    } else {
        Serial.println("[A2DP-AUDIO] WARNING: Earphones disconnected or scanning. Retrying connection...");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32-DUAL] JotCAD Wireless Synthesizer Node Booting...");

    // Initialize voice registers
    portENTER_CRITICAL(&synth_mux);
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        active_notes[i] = -1;
    }
    portEXIT_CRITICAL(&synth_mux);

#if ENABLE_VFS
    // 1. Initialize Wi-Fi connection
    Serial.printf("[Wi-Fi] Connecting to SSID: '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[Wi-Fi] Connected successfully!");
    Serial.printf("[Wi-Fi] ESP32 Local IP: %s\n", WiFi.localIP().toString().c_str());
#endif

    // 2. BLE Keyboard Scanner discovery phase
    Serial.println("\n[BLE-SCANNER] Initializing active scan to discover BLE MIDI controllers...");
    BLEDevice::init("JotCAD Synth Client");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new JotMidiDiscoveryCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    Serial.println("[BLE-SCANNER] Starting 5-second active discovery scan...");
    pBLEScan->start(5, false); // Synchronous block scan
    Serial.println("[BLE-SCANNER] Scan complete.");

    // 3. Initialize BLE-MIDI Client Keyboard Connection
    Serial.println("\n[BLE-MIDI] Bootstrapping BLE-MIDI Client connection...");
    BLEMidiClient.begin("JotCAD Synth Client");
    BLEMidiClient.setOnConnectCallback(OnBLEConnected);
    BLEMidiClient.setOnDisconnectCallback(OnBLEDisconnected);
    BLEMidiClient.setNoteOnCallback(OnBLENoteOn);
    BLEMidiClient.setNoteOffCallback(OnBLENoteOff);
    BLEMidiClient.setControlChangeCallback(OnBLEControlChange);
    Serial.println("[BLE-MIDI] Client actively searching and trying to connect to MIDI controller...");

    // Trigger active scan to connect BLE-MIDI keyboard
    int foundMidi = BLEMidiClient.scan();
    Serial.printf("[BLE-MIDI] Scan complete. Found %d MIDI-capable device(s) advertising.\n", foundMidi);
    if (foundMidi > 0) {
        Serial.println("[BLE-MIDI] Connecting to the first discovered MIDI keyboard...");
        if (BLEMidiClient.connect(0)) {
            Serial.println("[BLE-MIDI] >>> SUCCESS: Connection negotiated successfully! <<<");
        } else {
            Serial.println("[BLE-MIDI] ERROR: Connection failed. Will retry in background.");
        }
    }

    // 4. Initialize ESP32Synth Engine (Custom output at standard A2DP CD sample rate of 44100Hz)
    Serial.println("\n[SYNTH] Initializing Polyphonic Sound Engine...");
    synth.beginCustom(44100); // Perfect sample rate match for A2DP
    
    // Set standard ADSR instruments for all polyphonic voice channels
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        synth.setWave(i, WAVE_TRIANGLE); // Warm, clean triangle wave
        synth.setEnv(i, 20, 150, 192, 250); // ADSR: 20ms attack, 150ms decay, 75% sustain level, 250ms release stage
    }
    synth.setMasterVolume(255); // Set master volume to maximum 8-bit level
    Serial.println("[SYNTH] 16 Polyphonic triangle-envelope voices initialized.");

    // 5. Initialize Bluetooth A2DP Audio Streaming Source (earphones)
    Serial.println("\n[A2DP-AUDIO] Bootstrapping Bluetooth A2DP Audio transmitter...");
    a2dp_source.set_on_connection_state_changed(on_a2dp_connection_state);
    a2dp_source.set_on_audio_state_changed(on_a2dp_audio_state);
    a2dp_source.set_ssid_callback(my_ssid_callback);
    
    // Configure standard audio streaming configurations
    std::vector<const char*> target_names;
    if (strlen(BT_EARPHONES_NAME) > 0) {
        target_names.push_back(BT_EARPHONES_NAME);
        Serial.printf("[A2DP-AUDIO] Target earphone lock: '%s'\n", BT_EARPHONES_NAME);
    } else {
        Serial.println("[A2DP-AUDIO] Generic discovery mode. Will connect to first available audio receiver.");
    }
    
    // Start transmitting stereo audio stream
    a2dp_source.start(target_names, get_sound_data);
    a2dp_source.set_volume(100); // Set absolute volume to 100/127 to prevent muted default!
    Serial.println("[A2DP-AUDIO] Broadcasting A2DP streaming signals. Scanning for audio receivers...");

#if ENABLE_VFS
    // 6. Setup VFS Node Config
    Serial.println("\n[VFS] Bootstrapping VFS Client Node...");
    fs::VFS::Config config;
    config.id = DEVICE_NODE_ID;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    node = new fs::VFS(config);
    node->begin();
    Serial.println("[VFS] Node active!");
#else
    Serial.println("\n[VFS] VFS Mesh connection is temporarily DISABLED for standalone low-latency BLE/A2DP streaming.");
#endif
}

void loop() {
#if ENABLE_VFS
    if (node) {
        node->tick();
    }
#endif

    // 10-Second Keep-Alive Heartbeat, Diagnostics, and Bluetooth Self-Healing Reconnection Checkers
    static unsigned long last_diagnostic_tick = 0;
    if (millis() - last_diagnostic_tick > 10000) { // Every 10 seconds
        last_diagnostic_tick = millis();
        fs::json current = get_last_event();
        
        bool isBleConnected = BLEMidiClient.isConnected();
        bool isA2dpConnected = a2dp_source.is_connected();
        
        Serial.printf("[DIAGNOSTIC] Wi-Fi: %s | BLE Keyboard: %s | Earphones: %s | DSP Load: %.2f%% | Rendered Frames: %llu | Last Event: %s\n", 
                      (WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"), 
                      (isBleConnected ? "CONNECTED" : "DISCONNECTED"),
                      (isA2dpConnected ? "CONNECTED" : "DISCONNECTED"),
                      synth.getCPULoad(),
                      total_frames_rendered,
                      current.dump().c_str());
                      
        // A. Self-healing BLE-MIDI keyboard reconnection
        if (!isBleConnected) {
            Serial.println("[BLE-MIDI] Keyboard not connected. Retrying scan & pairing...");
            int found = BLEMidiClient.scan();
            if (found > 0) {
                Serial.println("[BLE-MIDI] Discovered keyboard. Initializing connection...");
                if (BLEMidiClient.connect(0)) {
                    Serial.println("[BLE-MIDI] >>> SUCCESS: Reconnected successfully! <<<");
                }
            }
        }
        
        // B. Self-healing Bluetooth Earphones connection logger
        verify_a2dp_connection();
    }
    delay(10); // Yield to prevent Core 1 CPU starvation
}
