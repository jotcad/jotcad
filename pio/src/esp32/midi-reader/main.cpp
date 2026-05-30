#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "vfs.h"
#include "secrets.h"

// BLE-MIDI and NimBLE scanning imports
#include <BLEMidi.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>

// Standard BLE MIDI Service UUID
static NimBLEUUID midiServiceUUID("03B80E5A-EDE8-4B33-A751-6CE34EC4C700");

// Verbose callbacks for advertised devices during discovery scan
class JotMidiDiscoveryCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("[BLE-SCANNER] Discovered device: ");
        Serial.printf("Name: '%s' | Addr: %s | RSSI: %d dBm", 
                      advertisedDevice->getName().c_str(),
                      advertisedDevice->getAddress().toString().c_str(),
                      advertisedDevice->getRSSI());
        
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(midiServiceUUID)) {
            Serial.print("  <== [MIDI-CAPABLE DEVICE DETECTED!]");
        }
        Serial.println();
    }
};


// Flag to temporarily disable VFS connection and network handshakes during testing
#define ENABLE_VFS 0

// Hardware Serial pins for physical MIDI input fallback (remapped dynamically per architecture)
#define MIDI_BAUD_RATE 31250
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define MIDI_SERIAL_PORT Serial1
#define MIDI_SERIAL_RX 4
#define MIDI_SERIAL_TX 5
#define MIDI_SERIAL_NAME "Serial1"
#else
#define MIDI_SERIAL_PORT Serial2
#define MIDI_SERIAL_RX 16
#define MIDI_SERIAL_TX 17
#define MIDI_SERIAL_NAME "Serial2"
#endif

fs::VFS *node = nullptr;

// Thread-safe state for the last received MIDI event
static fs::json last_midi_event = {{"type", "none"}};
static portMUX_TYPE event_mux = portMUX_INITIALIZER_UNLOCKED;

// Helper to update the last midi event thread-safely
void set_last_event(const fs::json& event) {
    portENTER_CRITICAL(&event_mux);
    last_midi_event = event;
    portEXIT_CRITICAL(&event_mux);
}

// Helper to copy the last midi event thread-safely
fs::json get_last_event() {
    fs::json event;
    portENTER_CRITICAL(&event_mux);
    event = last_midi_event;
    portEXIT_CRITICAL(&event_mux);
    return event;
}

// Broadcasts the MIDI event dynamically over the VFS mesh
void broadcast_vfs_event(const fs::json& event) {
    set_last_event(event);

    if (node) {
        fs::json selector = {{"path", "midi/events"}};
        Serial.printf("[VFS] Broadcasting event to mesh: %s\n", event.dump().c_str());
        int sent = node->notify(selector, event);
        Serial.printf("[VFS] Broadcast completed. Sent to %d peer(s).\n", sent);
    } else {
        Serial.println("[VFS] WARNING: VFS node not initialized, dropping broadcast.");
    }
}

// --- BLE-MIDI Callbacks ---

void OnBLEConnected() {
    Serial.println("\n======================================================================");
    Serial.println("[BLE-MIDI] >>> SUCCESS: Connected to MIDI Peripheral (Controller/Keyboard)! <<<");
    Serial.println("======================================================================");
}

void OnBLEDisconnected() {
    Serial.println("\n======================================================================");
    Serial.println("[BLE-MIDI] >>> WARNING: Disconnected from MIDI Peripheral. <<<");
    Serial.println("======================================================================");
}


void OnBLENoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] NOTE ON - Channel: %d, Note: %d, Velocity: %d (Timestamp: %u)\n", channel, note, velocity, timestamp);
    
    fs::json event = {
        {"type", "note_on"},
        {"channel", channel},
        {"note", note},
        {"velocity", velocity},
        {"source", "ble"},
        {"timestamp", timestamp}
    };
    broadcast_vfs_event(event);
}

void OnBLENoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] NOTE OFF - Channel: %d, Note: %d, Velocity: %d (Timestamp: %u)\n", channel, note, velocity, timestamp);
    
    fs::json event = {
        {"type", "note_off"},
        {"channel", channel},
        {"note", note},
        {"velocity", velocity},
        {"source", "ble"},
        {"timestamp", timestamp}
    };
    broadcast_vfs_event(event);
}

void OnBLEControlChange(uint8_t channel, uint8_t control, uint8_t value, uint16_t timestamp) {
    Serial.printf("[BLE-MIDI] CONTROL CHANGE - Channel: %d, CC: %d, Value: %d (Timestamp: %u)\n", channel, control, value, timestamp);
    
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


// --- Lightweight Hardware Serial MIDI Parser (Fallback Input) ---
enum MidiState {
    MIDI_STATE_IDLE,
    MIDI_STATE_BYTE1,
    MIDI_STATE_BYTE2
};

static MidiState midi_state = MIDI_STATE_IDLE;
static uint8_t midi_status = 0;
static uint8_t midi_data1 = 0;

void handle_hardware_midi_message(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t type_nibble = status & 0xF0;
    uint8_t channel = (status & 0x0F) + 1;
    
    fs::json event;
    bool valid = false;

    if (type_nibble == 0x90 && data2 > 0) { // Note On
        event = {
            {"type", "note_on"},
            {"channel", channel},
            {"note", data1},
            {"velocity", data2},
            {"source", "serial"}
        };
        valid = true;
        Serial.printf("[SERIAL-MIDI] Note On  - Ch:%d, Note:%d, Vel:%d\n", channel, data1, data2);
    } else if (type_nibble == 0x80 || (type_nibble == 0x90 && data2 == 0)) { // Note Off
        event = {
            {"type", "note_off"},
            {"channel", channel},
            {"note", data1},
            {"velocity", data2},
            {"source", "serial"}
        };
        valid = true;
        Serial.printf("[SERIAL-MIDI] Note Off - Ch:%d, Note:%d, Vel:%d\n", channel, data1, data2);
    } else if (type_nibble == 0xB0) { // CC
        event = {
            {"type", "control_change"},
            {"channel", channel},
            {"control", data1},
            {"value", data2},
            {"source", "serial"}
        };
        valid = true;
        Serial.printf("[SERIAL-MIDI] CC       - Ch:%d, CC:%d, Val:%d\n", channel, data1, data2);
    }

    if (valid) {
        broadcast_vfs_event(event);
    }
}

void parse_serial_midi_byte(uint8_t b) {
    if (b >= 0x80) { // Status byte
        if (b < 0xF0) { // Voice messages only
            midi_status = b;
            midi_state = MIDI_STATE_BYTE1;
        } else {
            midi_state = MIDI_STATE_IDLE;
        }
    } else { // Data byte
        if (midi_state == MIDI_STATE_BYTE1) {
            midi_data1 = b;
            uint8_t type_nibble = midi_status & 0xF0;
            if (type_nibble == 0xC0 || type_nibble == 0xD0) {
                handle_hardware_midi_message(midi_status, midi_data1, 0);
                midi_state = MIDI_STATE_IDLE;
            } else {
                midi_state = MIDI_STATE_BYTE2;
            }
        } else if (midi_state == MIDI_STATE_BYTE2) {
            handle_hardware_midi_message(midi_status, midi_data1, b);
            midi_state = MIDI_STATE_IDLE;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[ESP32-C3] JotCAD VFS MIDI Reader Node Booting...");

    // 1. Initialize Wi-Fi connection
    Serial.printf("[Wi-Fi] Connecting to SSID: '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[Wi-Fi] Connected successfully!");
    Serial.printf("[Wi-Fi] ESP32-C3 Local IP: %s\n", WiFi.localIP().toString().c_str());

    // 2. BLE Device Scan & Discovery Phase (acting as active scanner)
    Serial.println("\n[BLE-SCANNER] Initializing active scan to discover available BLE devices...");
    NimBLEDevice::init("JotCAD MIDI Client");
    NimBLEScan* pNimBLEScan = NimBLEDevice::getScan();
    pNimBLEScan->setAdvertisedDeviceCallbacks(new JotMidiDiscoveryCallbacks());
    pNimBLEScan->setActiveScan(true);
    pNimBLEScan->setInterval(100);
    pNimBLEScan->setWindow(99);
    
    Serial.println("[BLE-SCANNER] Starting 5-second active discovery scan...");
    pNimBLEScan->start(5, false); // Synchronous block scan for 5 seconds
    Serial.println("[BLE-SCANNER] Scan complete.");


    // 3. Initialize BLE-MIDI Client Stack & Connect
    Serial.println("\n[BLE-MIDI] Bootstrapping BLE-MIDI Client...");
    BLEMidiClient.begin("JotCAD MIDI Client");
    
    // Wire up connection & MIDI parser client callbacks
    BLEMidiClient.setOnConnectCallback(OnBLEConnected);
    BLEMidiClient.setOnDisconnectCallback(OnBLEDisconnected);
    BLEMidiClient.setNoteOnCallback(OnBLENoteOn);
    BLEMidiClient.setNoteOffCallback(OnBLENoteOff);
    BLEMidiClient.setControlChangeCallback(OnBLEControlChange);
    Serial.println("[BLE-MIDI] Client successfully bootstrapped.");

    // Trigger active scan and connect to first found MIDI device
    Serial.println("[BLE-MIDI] Actively scanning for advertising BLE-MIDI devices...");
    int found = BLEMidiClient.scan();
    Serial.printf("[BLE-MIDI] Scan complete. Found %d MIDI-capable device(s) advertising.\n", found);
    
    if (found > 0) {
        for (int i = 0; i < found; i++) {
            auto* dev = BLEMidiClient.getScannedDevice(i);
            if (dev) {
                Serial.printf("[BLE-MIDI]   -> Device %d: Name: '%s' | Address: %s | RSSI: %d dBm\n",
                              i, dev->getName().c_str(), dev->getAddress().toString().c_str(), dev->getRSSI());
            }
        }
        
        Serial.println("[BLE-MIDI] Initializing connection to Device 0...");
        auto* target = BLEMidiClient.getScannedDevice(0);
        if (target) {
            Serial.printf("[BLE-MIDI] Target device selected: Name: '%s' | Address: %s\n", 
                          target->getName().c_str(), target->getAddress().toString().c_str());
        }
        
        bool connected = BLEMidiClient.connect(0);
        if (connected) {
            Serial.println("[BLE-MIDI] >>> SUCCESS: Connection negotiated successfully! <<<");
        } else {
            Serial.println("[BLE-MIDI] ERROR: Connection failed. Will automatically retry in loop.");
        }
    } else {
        Serial.println("[BLE-MIDI] WARNING: No MIDI-capable devices found advertising. Connection deferred to auto-retry loop.");
    }


    // 3. Initialize Physical Serial MIDI Port (dynamically mapped hardware port)
    MIDI_SERIAL_PORT.begin(MIDI_BAUD_RATE, SERIAL_8N1, MIDI_SERIAL_RX, MIDI_SERIAL_TX);
    Serial.printf("[SERIAL-MIDI] Listening on Hardware %s (RX=%d, TX=%d) at %d baud.\n", 
                  MIDI_SERIAL_NAME, MIDI_SERIAL_RX, MIDI_SERIAL_TX, MIDI_BAUD_RATE);

#if ENABLE_VFS
#ifndef DEVICE_NODE_ID
#error "DEVICE_NODE_ID macro is not defined! Please define sysenv.DEVICE_NODE_ID in your environment."
#endif

    // 4. Setup VFS Node Config
    Serial.println("[VFS] Bootstrapping VFS Client Node...");
    fs::VFS::Config config;
    config.id = DEVICE_NODE_ID;
    config.enabled_features = fs::VFS_HANDSHAKE | fs::VFS_FULFILLMENT | fs::VFS_PUBLICATION | fs::VFS_SUBSCRIPTION;
    config.neighbors = {MESH_NEIGHBOR_URL};
    
    delay(2000); // Settle time
    
    // Probe VFS Neighbor Gateway direct sanity
    Serial.printf("[VFS] Probing neighbor router %s/health...\n", MESH_NEIGHBOR_URL);
    HTTPClient probe;
    probe.begin(String(MESH_NEIGHBOR_URL) + "/health");
    int probeCode = probe.GET();
    if (probeCode > 0) {
        Serial.printf("[VFS] Gateway Health Check: SUCCESS (Status Code %d)\n", probeCode);
    } else {
        Serial.printf("[VFS] Gateway Health Check: FAILED: %s (%d)\n", 
                      probe.errorToString(probeCode).c_str(), probeCode);
    }
    probe.end();

    node = new fs::VFS(config);

    // Register active handler so other nodes on the mesh can fetch the last event
    node->register_op("midi/status", [](const fs::json& params, fs::VFSResponseWriter *response) {
        fs::json last_event = get_last_event();
        Serial.printf("[VFS-RPC] Received read request for 'midi/status'. Replying with: %s\n", last_event.dump().c_str());
        response->send(200, "application/json", last_event.dump().c_str());
    }, {{"output", "object"}});

    node->begin();
    Serial.println("[VFS] Node fully active! Registering VFS_READY signal.");
    Serial.println("VFS_READY");
#else
    Serial.println("[VFS] VFS Mesh connection is temporarily DISABLED for standalone BLE testing.");
#endif
}

void loop() {
#if ENABLE_VFS
    // Standard event tick dispatch for VFS networking (drains sockets & polls connections)
    if (node) {
        node->tick();
    }
#endif

    // 1. Process incoming physical Serial-MIDI streams
    while (MIDI_SERIAL_PORT.available() > 0) {
        uint8_t b = MIDI_SERIAL_PORT.read();
        parse_serial_midi_byte(b);
    }

    // 2. Keep-alive monitor, diagnostics, and auto-reconnection checker
    static unsigned long last_diagnostic_tick = 0;
    if (millis() - last_diagnostic_tick > 10000) { // Every 10 seconds
        last_diagnostic_tick = millis();
        fs::json current = get_last_event();
        bool isConnected = BLEMidiClient.isConnected();
        Serial.printf("[DIAGNOSTIC] Wi-Fi Status: %s | BLE Connected: %s | Last Event: %s\n", 
                      (WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"), 
                      (isConnected ? "YES" : "NO"),
                      current.dump().c_str());
        
        // Auto-reconnect if disconnected
        if (!isConnected) {
            Serial.println("[BLE-MIDI] Device not connected. Attempting auto-scan and reconnection...");
            int found = BLEMidiClient.scan();
            Serial.printf("[BLE-MIDI] Scan complete. Found %d MIDI-capable device(s) advertising.\n", found);
            
            if (found > 0) {
                for (int i = 0; i < found; i++) {
                    auto* dev = BLEMidiClient.getScannedDevice(i);
                    if (dev) {
                        Serial.printf("[BLE-MIDI]   -> Device %d: Name: '%s' | Address: %s | RSSI: %d dBm\n",
                                      i, dev->getName().c_str(), dev->getAddress().toString().c_str(), dev->getRSSI());
                    }
                }
                
                Serial.println("[BLE-MIDI] Initializing connection to Device 0...");
                auto* target = BLEMidiClient.getScannedDevice(0);
                if (target) {
                    Serial.printf("[BLE-MIDI] Target device selected: Name: '%s' | Address: %s\n", 
                                  target->getName().c_str(), target->getAddress().toString().c_str());
                }
                
                if (BLEMidiClient.connect(0)) {
                    Serial.println("[BLE-MIDI] >>> SUCCESS: Reconnected successfully! <<<");
                } else {
                    Serial.println("[BLE-MIDI] ERROR: Reconnection attempt failed.");
                }
            } else {
                Serial.println("[BLE-MIDI] Scan found no advertising MIDI devices. Will retry in 10 seconds.");
            }
        }

    }
}
