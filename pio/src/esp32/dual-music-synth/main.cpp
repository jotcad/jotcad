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

// Synthesizer imports
#include "ESP32Synth.h"
#include "driver/i2s.h"

static uint64_t total_frames_rendered = 0;

// Define I2S Pins for the LMD2718 + NS4168 Board. 
// Standard ESP32 hardware I2S pins:
#define I2S_BCLK GPIO_NUM_26
#define I2S_WS GPIO_NUM_25
#define I2S_DOUT GPIO_NUM_22
#define I2S_SD_PIN GPIO_NUM_23
#define I2S_NUM I2S_NUM_0

void init_i2s() {
    Serial.println("[I2S] Initializing standard I2S mode for NS4168 DAC/Amp (Legacy Driver)...");
    
    // Enable/unmute the amplifier via the SD (Shutdown/Mute) pin
    pinMode(I2S_SD_PIN, OUTPUT);
    digitalWrite(I2S_SD_PIN, HIGH);
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Failed to install driver: %s\n", esp_err_to_name(err));
        return;
    }
    
    err = i2s_set_pin(I2S_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Failed to set pins: %s\n", esp_err_to_name(err));
        return;
    }
    Serial.println("[I2S] Standard I2S initialized successfully!");
}

void i2s_output_callback(int16_t* samples, int numSamples) {
    total_frames_rendered += numSamples;

    // Allocate stack buffer for stereo conversion
    int16_t stereoBuffer[numSamples * 2];
    for (int i = 0; i < numSamples; i++) {
        stereoBuffer[i * 2] = samples[i];
        stereoBuffer[i * 2 + 1] = samples[i];
    }

    size_t bytes_written = 0;
    i2s_write(I2S_NUM, stereoBuffer, sizeof(stereoBuffer), &bytes_written, portMAX_DELAY);
}

// 16-note polyphonic voice allocation mapper
#define MAX_POLYPHONY 16
static int active_notes[MAX_POLYPHONY];
static int next_voice_index = 0;
static portMUX_TYPE synth_mux = portMUX_INITIALIZER_UNLOCKED;

// --- Synthesizer Globals ---
ESP32Synth synth;

// Available Instrument Configurations
enum InstrumentType {
    INST_POLY_SYNTH = 0,
    INST_BRIGHT_LEAD,
    INST_PLUCKED_HARP,
    INST_CHIPTUNE_BASS,
    INST_THEREMIN,
    INST_COUNT
};

static int current_instrument = -1; // Force initial selection

void select_instrument(int inst_idx) {
    if (inst_idx < 0 || inst_idx >= INST_COUNT) return;
    if (inst_idx == current_instrument) return; // Prevent redundant updates
    
    current_instrument = inst_idx;
    
    portENTER_CRITICAL(&synth_mux);
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        switch (current_instrument) {
            case INST_POLY_SYNTH:
                synth.setWave(i, WAVE_TRIANGLE);
                synth.setEnv(i, 20, 150, 192, 250);
                break;
            case INST_BRIGHT_LEAD:
                synth.setWave(i, WAVE_SAW);
                synth.setEnv(i, 10, 200, 220, 300);
                break;
            case INST_PLUCKED_HARP:
                synth.setWave(i, WAVE_TRIANGLE);
                synth.setEnv(i, 5, 250, 0, 150);
                break;
            case INST_CHIPTUNE_BASS:
                synth.setWave(i, WAVE_PULSE);
                synth.setPulseWidth(i, 50); // ~20% duty cycle (standard 8-bit scale 0-255)
                synth.setEnv(i, 10, 100, 128, 100);
                break;
            case INST_THEREMIN:
                synth.setWave(i, WAVE_SINE);
                synth.setEnv(i, 100, 300, 255, 400);
                break;
        }
    }
    portEXIT_CRITICAL(&synth_mux);

    const char* names[] = {
        "Classic Poly Synth",
        "Bright Lead (Sawtooth)",
        "Plucked Harp",
        "Chiptune Bass (Pulse)",
        "Theremin / Flute (Sine)"
    };
    Serial.printf("[SYNTH] >>> ACTIVE INSTRUMENT CHANGED: %s <<<\n", names[current_instrument]);
}






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
    
    // Map CC 7 (typically volume, used as a selector knob here) to change active instrument
    if (control == 7) {
        int inst_idx = (int)value * INST_COUNT / 128; // Maps 0-127 values to 0-4
        select_instrument(inst_idx);
    }
    
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

    // 4. Initialize ESP32Synth Engine (Custom output at standard CD sample rate of 44100Hz)
    Serial.println("\n[SYNTH] Initializing Polyphonic Sound Engine...");
    init_i2s();
    synth.beginCustom(44100, i2s_output_callback);
    
    // Initialize default instrument (Classic Poly Synth)
    select_instrument(INST_POLY_SYNTH);
    synth.setMasterVolume(255); // Set master volume to maximum 8-bit level



#if ENABLE_VFS
    // 6. Setup VFS Node Config
    Serial.println("\n[VFS] Bootstrapping VFS Client Node...");
    fs::VFS::Config config;
    char unique_id[64];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(unique_id, sizeof(unique_id), "%s-%06X", DEVICE_NODE_ID, (uint32_t)(mac & 0xFFFFFF));
    config.id = unique_id;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    node = new fs::VFS(config);
    node->begin();
    Serial.println("[VFS] Node active!");
#else
    Serial.println("\n[VFS] VFS Mesh connection is temporarily DISABLED for standalone low-latency BLE/A2DP streaming.");
#endif

    // Play a startup chime to verify the speaker is working
    Serial.println("[SYNTH] Playing startup chime...");
    delay(500); // Wait for the amplifier to settle
    
    // Play Note C4 (MIDI 60 -> 261.63 Hz -> 26163 centihertz)
    synth.noteOn(0, 26163, 127);
    delay(250);
    synth.noteOff(0);
    delay(50);
    
    // Play Note E4 (MIDI 64 -> 329.63 Hz -> 32963 centihertz)
    synth.noteOn(0, 32963, 127);
    delay(250);
    synth.noteOff(0);
    delay(50);
    
    // Play Note G4 (MIDI 67 -> 392.00 Hz -> 39200 centihertz)
    synth.noteOn(0, 39200, 127);
    delay(250);
    synth.noteOff(0);
    delay(50);
    
    // Play Note C5 (MIDI 72 -> 523.25 Hz -> 52325 centihertz)
    synth.noteOn(0, 52325, 127);
    delay(500);
    synth.noteOff(0);
    
    Serial.println("[SYNTH] Startup chime complete.");
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
        
        Serial.printf("[DIAGNOSTIC] Wi-Fi: %s | BLE Keyboard: %s | Audio: I2S_NS4168 | DSP Load: %.2f%% | Rendered Frames: %llu | Last Event: %s\n", 
                      (WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"), 
                      (isBleConnected ? "CONNECTED" : "DISCONNECTED"),
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
    }
    delay(10); // Yield to prevent Core 1 CPU starvation
}
