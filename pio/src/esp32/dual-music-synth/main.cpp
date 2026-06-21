#include <Arduino.h>
#define ENABLE_VFS 1
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

// BLE-MIDI and standard BLE scanning imports
#include <BLEMidi.h>
#ifdef MIDI_USE_NIMBLE
#include <NimBLEDevice.h>
#else
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#endif

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

// --- Wavetable Data Definitions ---
const uint8_t buzzy_wavetable[256] = {
  128, 148, 168, 186, 202, 215, 225, 232, 236, 238, 237, 234, 229, 222, 214, 205,
  195, 185, 175, 165, 155, 146, 137, 128, 120, 112, 104,  97,  90,  84,  78,  73,
   69,  65,  62,  60,  58,  57,  57,  57,  58,  60,  62,  65,  68,  72,  76,  80,
   85,  89,  94,  98, 102, 106, 110, 113, 117, 120, 123, 125, 127, 128, 128, 128,
  128, 127, 125, 123, 120, 117, 113, 110, 106, 102,  98,  94,  89,  85,  80,  76,
   72,  68,  65,  62,  60,  58,  57,  57,  57,  58,  60,  62,  65,  69,  73,  78,
   84,  90,  97, 104, 112, 120, 128, 137, 146, 155, 165, 175, 185, 195, 205, 214,
  222, 229, 234, 237, 238, 236, 232, 225, 215, 202, 186, 168, 148, 128, 107,  87,
   69,  53,  40,  30,  23,  19,  17,  18,  21,  26,  33,  41,  50,  60,  70,  80,
   90, 100, 109, 118, 127, 135, 143, 151, 158, 164, 170, 175, 179, 183, 186, 188,
  190, 191, 191, 191, 190, 188, 186, 183, 179, 175, 170, 164, 158, 151, 143, 135,
  127, 118, 109, 100,  90,  80,  70,  60,  50,  41,  33,  26,  21,  18,  17,  19,
   23,  30,  40,  53,  69,  87, 107, 128
};

const uint8_t hollow_wavetable[256] = {
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  250, 240, 230, 220, 210, 200, 190, 180, 170, 160, 150, 140, 130, 120, 110, 100,
   90,  80,  70,  60,  50,  40,  30,  20,  10,   5,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    5,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100, 110, 120, 130, 140, 150,
   160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 255, 255, 255, 255, 255, 255,
   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

// --- Custom Wave DSP Callbacks ---
void fm_custom_wave(Voice* vo, int32_t* mixBuffer, int samples, int32_t startEnv, int32_t envStep) {
    int32_t currentEnv = startEnv;
    int32_t volBase = ((uint32_t)vo->vol * vo->trmModGain) >> 8;
    uint32_t ph = vo->phase;
    uint32_t inc = vo->phaseInc + vo->vibOffset;
    uint32_t modPhase = ph * 2; 

    for (int i = 0; i < samples; i++) {
        int32_t envSafe = currentEnv >> 14;
        envSafe &= ~(envSafe >> 31);
        int32_t finalVol = (int32_t)((envSafe * volBase) >> 14);
        
        // Simple FM carrier modulation by 2x modulator frequency
        int16_t modVal = sineLUT[modPhase >> SINE_SHIFT];
        uint32_t modulatedCarrierPhase = ph + ((int32_t)modVal * 4);
        
        mixBuffer[i] += (sineLUT[modulatedCarrierPhase >> SINE_SHIFT] * finalVol) >> 16;
        
        ph += inc;
        modPhase += inc * 2;
        currentEnv += envStep;
    }
    vo->phase = ph;
}

// --- Synthesizer Globals ---
ESP32Synth synth;

struct DynamicInstrument {
    std::string name;
    int8_t wave;
    uint8_t pulse_width;
    uint16_t attack;
    uint16_t decay;
    uint8_t sustain;
    uint16_t release;
    uint16_t wavetable_id;
    uint16_t sample_id;
    uint16_t stream_id;
    uint16_t volume_scale; // To balance perceived loudness differences (e.g. 0-255)
};

static std::vector<DynamicInstrument> dynamic_instruments;
static int current_instrument = -1; // Force initial selection

void init_default_instruments() {
    dynamic_instruments = {
        {"Classic Poly Synth", WAVE_TRIANGLE, 128, 20, 150, 192, 250, 0, 0, 0, 240},
        {"Bright Lead (Sawtooth)", WAVE_SAW, 128, 10, 200, 220, 300, 0, 0, 0, 120},
        {"Plucked Harp", WAVE_TRIANGLE, 128, 5, 250, 0, 150, 0, 0, 0, 240},
        {"Chiptune Bass (Pulse)", WAVE_PULSE, 50, 10, 100, 128, 100, 0, 0, 0, 110},
        {"Theremin / Flute (Sine)", WAVE_SINE, 128, 100, 300, 255, 400, 0, 0, 0, 255}
    };
}

void select_instrument(int inst_idx) {
    if (dynamic_instruments.empty()) return;
    if (inst_idx < 0) inst_idx = 0;
    if (inst_idx >= (int)dynamic_instruments.size()) inst_idx = (int)dynamic_instruments.size() - 1;
    if (inst_idx == current_instrument) return; // Prevent redundant updates
    
    current_instrument = inst_idx;
    const auto& inst = dynamic_instruments[current_instrument];
    
    portENTER_CRITICAL(&synth_mux);
    for (int i = 0; i < MAX_POLYPHONY; i++) {
        synth.setWave(i, (WaveType)inst.wave);
        if (inst.wave == WAVE_PULSE) {
            synth.setPulseWidth(i, inst.pulse_width);
        }
        else if (inst.wave == WAVE_WAVETABLE) {
            if (inst.wavetable_id < MAX_WAVETABLES) {
                synth.setWavetable(i, synth.wavetables[inst.wavetable_id].data, 
                                   synth.wavetables[inst.wavetable_id].size, 
                                   (BitDepth)synth.wavetables[inst.wavetable_id].depth);
            }
        }
        else if (inst.wave == WAVE_SAMPLE) {
            synth.setSample(i, inst.sample_id);
        }
        else if (inst.wave == WAVE_STREAM) {
            synth.voices[i].streamTrackId = inst.stream_id;
        }
        else if (inst.wave == WAVE_CUSTOM) {
            synth.setCustomWave(i, fm_custom_wave);
        }
        synth.setEnv(i, inst.attack, inst.decay, inst.sustain, inst.release);
    }
    portEXIT_CRITICAL(&synth_mux);

    Serial.printf("[SYNTH] >>> ACTIVE INSTRUMENT CHANGED: %s (Type: %d, VolScale: %d) <<<\n", 
                  inst.name.c_str(), inst.wave, inst.volume_scale);
}






// Standard BLE MIDI Service UUID
static BLEUUID midiServiceUUID("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");

// --- BLE-MIDI Discovery Scan Callbacks ---
#ifdef MIDI_USE_NIMBLE
class JotMidiDiscoveryCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("[BLE-SCANNER] Discovered device: ");
        Serial.printf("Name: '%s' | Addr: %s | RSSI: %d dBm", 
                      advertisedDevice->getName().c_str(),
                      advertisedDevice->getAddress().toString().c_str(),
                      advertisedDevice->getRSSI());
        
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(midiServiceUUID)) {
            Serial.print("  <== [MIDI-CAPABLE KEYBOARD DETECTED!]");
        }
        Serial.println();
    }
};
#else
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
#endif

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
    if (node) {
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
    
    // Scale volume based on the active instrument's volume scale to balance loudness differences
    if (current_instrument >= 0 && current_instrument < (int)dynamic_instruments.size()) {
        uint32_t scale = dynamic_instruments[current_instrument].volume_scale;
        synthVolume = (uint8_t)(((uint32_t)synthVolume * scale) / 255);
    }
    
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
        select_instrument(value);
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


void update_dynamic_instruments(const fs::json& config) {
    if (!config.contains("instruments") || !config["instruments"].is_array()) {
        Serial.println("[SYNTH] Invalid instrument config JSON payload");
        return;
    }
    
    std::vector<DynamicInstrument> new_insts;
    for (auto const& item : config["instruments"]) {
        try {
            DynamicInstrument inst;
            inst.name = item.value("name", "Unnamed");
            inst.wave = item.value("wave", -2); // WAVE_TRIANGLE default
            inst.pulse_width = item.value("pulse_width", 128);
            inst.attack = item.value("attack", 20);
            inst.decay = item.value("decay", 150);
            inst.sustain = item.value("sustain", 192);
            inst.release = item.value("release", 250);
            inst.wavetable_id = item.value("wavetable_id", 0);
            inst.sample_id = item.value("sample_id", 0);
            inst.stream_id = item.value("stream_id", 0);
            inst.volume_scale = item.value("volume_scale", 255);
            new_insts.push_back(inst);
        } catch (...) {
            Serial.println("[SYNTH] Failed parsing individual instrument parameters");
        }
    }
    
    if (!new_insts.empty()) {
        portENTER_CRITICAL(&synth_mux);
        dynamic_instruments = new_insts;
        current_instrument = -1; // Force re-evaluation
        portEXIT_CRITICAL(&synth_mux);
        
        Serial.printf("[SYNTH] Successfully loaded %d dynamic instruments from VFS!\n", (int)dynamic_instruments.size());
        select_instrument(0);
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

    init_default_instruments();

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
    
    // Register custom wavetables for WAVE_WAVETABLE engine type
    synth.registerWavetable(0, buzzy_wavetable, 256, BITS_8);
    synth.registerWavetable(1, hollow_wavetable, 256, BITS_8);

    // Initialize default instrument (Classic Poly Synth)
    select_instrument(0);
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

    // Register notify listener for dynamic config updates
    node->register_notify_handler([](const fs::Selector& sel, const fs::json& payload) {
        if (sel.path == "instrument/config") {
            Serial.println("[VFS] Received instrument/config notification!");
            update_dynamic_instruments(payload);
        }
    });

    // Register synth play endpoint for UX keyboard
    node->register_op("synth/note", [](const fs::json& params, fs::VFSResponseWriter* response) {
        if (params.contains("note") && params.contains("type")) {
            int note = params["note"].get<int>();
            std::string type = params["type"].get<std::string>();
            int velocity = params.value("velocity", 127);
            
            if (type == "on") {
                double freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
                uint32_t freqCentiHz = (uint32_t)(freq * 100.0);
                portENTER_CRITICAL(&synth_mux);
                int voice = next_voice_index;
                next_voice_index = (next_voice_index + 1) % MAX_POLYPHONY;
                active_notes[voice] = note;
                uint8_t synthVolume = (velocity > 127) ? 255 : (velocity << 1); 
                
                // Scale volume based on the active instrument's volume scale to balance loudness differences
                if (current_instrument >= 0 && current_instrument < (int)dynamic_instruments.size()) {
                    uint32_t scale = dynamic_instruments[current_instrument].volume_scale;
                    synthVolume = (uint8_t)(((uint32_t)synthVolume * scale) / 255);
                }
                
                synth.noteOn(voice, freqCentiHz, synthVolume);
                portEXIT_CRITICAL(&synth_mux);
            } else if (type == "off") {
                portENTER_CRITICAL(&synth_mux);
                for (int i = 0; i < MAX_POLYPHONY; i++) {
                    if (active_notes[i] == note) {
                        synth.noteOff(i);
                        active_notes[i] = -1;
                    }
                }
                portEXIT_CRITICAL(&synth_mux);
            }
            response->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            response->send(400, "application/json", "{\"error\":\"Missing params\"}");
        }
    });

    node->register_op("synth/control", [](const fs::json& params, fs::VFSResponseWriter* response) {
        if (params.contains("control") && params.contains("value")) {
            int control = params["control"].get<int>();
            int value = params["value"].get<int>();
            if (control == 7) {
                select_instrument(value);
            }
            response->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            response->send(400, "application/json", "{\"error\":\"Missing params\"}");
        }
    });

    // Subscribe to live updates
    node->subscribe(fs::Selector("instrument/config"), 2147483647LL);
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

        // B. Resilient pull: Fetching instrument configuration from VFS if not already pulled
        static bool initial_pull_done = false;
        if (!initial_pull_done && WiFi.status() == WL_CONNECTED && node) {
            Serial.println("[VFS] Resilient pull: Fetching instrument configuration from VFS...");
            fs::json initial_config = node->read_selector(fs::Selector("instrument/config"));
            if (!initial_config.empty()) {
                update_dynamic_instruments(initial_config);
                initial_pull_done = true;
            }
        }
    }
    delay(10); // Yield to prevent Core 1 CPU starvation
}
