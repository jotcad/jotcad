# JotCAD ESP32 Dual Music Synthesizer Node

> [!IMPORTANT]
> **HARDWARE COMPATIBILITY NOTICE**:
> *   **Supported Hardware**: Classic original dual-core ESP32 microcontrollers (e.g., **ESP32-WROOM-32**, **ESP32-WROVER**, or **ESP32-DevKitC**).
> *   **Unsupported Hardware**: **ESP32-S3**, **ESP32-S2**, **ESP32-C3**, and **ESP32-C6** are **NOT supported** by this PlatformIO environment configuration.

This directory contains the firmware for the **JotCAD ESP32 Dual Music Synthesizer Node** (`esp32_dual_music_synth`), a dedicated high-fidelity music synthesizer targeting classic dual-core ESP32 microcontrollers. 

This node connects asynchronously as a **BLE-MIDI Client** to your MIDI keyboard (e.g., `SMK25Mini`), maps key velocity to polyphonic voice channels, synthesizes real-time stereo audio (44.1kHz) in memory, and outputs it directly via the **I2S bus** to the **LMD2718 + NS4168 Dual-Chip Stereo Sound Board**.

---

## 1. Technical Architecture

Running real-time sound synthesis concurrently with active BLE-MIDI wireless protocols is handled using a strict **Dual-Core scheduling allocation** to prevent buffer underruns and audio stuttering:

```mermaid
graph TD
    subgraph Core 1 (Network & Control Thread)
        BLE[BLE-MIDI Client] -->|Midi Events| VM[Polyphonic Voice Mapper]
        VM -->|Note On/Off| EN[ADSR Envelopes]
        HB[10s Reconnection Heartbeat] -->|Self-Healing| BLE
    end

    subgraph Core 0 (I2S Audio Thread)
        I2S[I2S Audio Output] -->|PCM Audio Push Callback| SC[I2S Output Callback]
        SC -->|Render Demand| ES[ESP32Synth Custom Output Generator]
        EN -->|Wave & Frequency State| ES
    end

    subgraph Audio Board
        SC -->|Wired I2S Stereo Signal| AMP[LMD2718 + NS4168 Amplifier]
        AMP -->|Stereo Sound| SPK[Speakers]
    end

    subgraph Keyboard
        KB[SMK25Mini Keyboard] -->|Wireless BLE-MIDI| BLE
    end
    
    style Core 1 fill:#2b2d42,stroke:#8d99ae,stroke-width:2px,color:#fff
    style Core 0 fill:#1d3557,stroke:#457b9d,stroke-width:2px,color:#fff
    style Audio Board fill:#003049,stroke:#d62828,stroke-width:2px,color:#fff
    style Keyboard fill:#003049,stroke:#fcbf49,stroke-width:2px,color:#fff
```

### Core Allocation Breakdown:
*   **Core 1 (Control Plane)**: Manages incoming wireless BLE-MIDI packets from the keyboard, performs active device scans, parses note-on/note-off actions, executes the 16-note round-robin polyphonic voice mapping, and runs the 10-second automatic reconnection heartbeat.
*   **Core 0 (Audio Plane)**: Dedicated exclusively to high-priority audio generation. The custom audio output task calls the synthesizer to generate stereo samples at exactly 44.1kHz, which are then written directly to the I2S hardware peripheral.

---

## 2. Wiring & Pin Connection Mapping

The **LMD2718 + NS4168** module features **7 pins**. Connect them to your ESP32 board according to the table below:

| Board Pin Label | ESP32 GPIO Pin | Function | Status |
| :--- | :--- | :--- | :--- |
| **lrclk** | **GPIO 25** | I2S Word Select (LRCK / WS) | **Connect** |
| **bclk** | **GPIO 26** | I2S Bit Clock (BCLK) | **Connect** |
| **sda** | **GPIO 22** | I2S Serial Data (DOUT from ESP32) | **Connect** |
| **vcc** | **5V** (or 3.3V) | Power supply (5V is recommended for max volume) | **Connect** |
| **gnd** | **GND** | Ground (common ground) | **Connect** |
| **data** | *NC / Disconnected* | PDM Microphone Data Out | *Leave Disconnected* (not used by synth) |
| **clk** | *NC / Disconnected* | PDM Microphone Clock In | *Leave Disconnected* (not used by synth) |

---

## 3. Polyphony & Synthesizer Settings

The synthesizer is configured with up to 16 concurrent voices using a thread-safe round-robin allocation and features a startup chime verification (playing sequential C-E-G-C notes on boot).

### Dynamic Instrument Loading & Selection (MIDI CC 7)
The synthesizer dynamically registers a WebSocket VFS provider mapping the path `instrument/config`. On startup and network publications, the firmware pulls this configuration to hot-swap active instrument voice parameters. 

By default, the config loads 128 General MIDI instrument definitions covering all **five generator engine types** (Basic oscillators, Wavetables, Samples, Streams, and Custom wave callbacks/FM) with individual **volume scaling factor leveling**.

You can dynamically switch the active voice preset by sending a Control Change **CC 7** message. The CC 7 value (0-127) maps **directly to the active instrument index** in the configured list (capped at the maximum available index).

For detailed configurations, wave tables, sample/stream tracks, custom carrier/modulator parameters, and volume leveling formulas, see the [SYNTH_MIDI_SPECIFICATION.md](file:///home/brian/github/jotcad/docs/SYNTH_MIDI_SPECIFICATION.md) in the project documentation directory.

---

## 4. How to Compile and Flash

Connect your classic dual-core ESP32 board to your computer via USB.

### A. Automatic Flash Script
Run the pre-configured flashing script which will build the project and upload the compiled firmware:
```bash
chmod +x pio/flash_esp32_dual_music_synth.sh
./pio/flash_esp32_dual_music_synth.sh
```

### B. Manual Compilation
To compile the node manually using the PlatformIO command-line tool, execute:
```bash
pio run -d pio -e esp32_dual_music_synth
```

---

## 5. Serial Diagnostics & Diagnostics Heartbeat

To monitor the active connection handshakes, wireless scans, and real-time state of the synthesizer node, run the serial monitor script (defaults to 115200 baud):
```bash
chmod +x pio/monitor_esp32_dual_music_synth.sh
./pio/monitor_esp32_dual_music_synth.sh
```

Every 10 seconds, the synthesizer prints a comprehensive diagnostic line to the serial output:
```text
[DIAGNOSTIC] Wi-Fi: DISCONNECTED | BLE Keyboard: CONNECTED | Audio: I2S_NS4168 | DSP Load: 2.50% | Rendered Frames: 44100 | Last Event: {"type":"note_on","channel":0,"note":60,"velocity":100,"source":"ble"}
```

*   **Self-Healing Reconnection**: If the keyboard is turned off or goes out of range, the background scanner detects the loss of connection and will automatically trigger active background scans every 10 seconds to re-establish the connection.
