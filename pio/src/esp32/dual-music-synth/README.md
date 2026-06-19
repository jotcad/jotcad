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

### Instrument Selection (MIDI CC 7)
You can dynamically switch between **5 different instrument presets** by turning the Control Change **CC 7** knob (mapping standard `0-127` values):

| Instrument Index | CC 7 Value Range | Preset Name | Waveform | ADSR Configuration | Sound Profile |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | `0` – `25` | **Classic Poly Synth** | Triangle | `20, 150, 192, 250` | Snappy, warm analog synthesizer default |
| **1** | `26` – `51` | **Bright Lead** | Sawtooth | `10, 200, 220, 300` | Bright, rich leads with a classic sawtooth bite |
| **2** | `52` – `76` | **Plucked Harp** | Triangle | `5, 250, 0, 150` | Plucked acoustic strings with no sustain stage |
| **3** | `77` – `101` | **Chiptune Bass** | Pulse | `10, 100, 128, 100` | Vintage 8-bit square bass with 20% duty cycle |
| **4** | `102` – `127` | **Theremin / Flute** | Sine | `100, 300, 255, 400` | Eerie, sweeping sine wave with a slow attack |

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
