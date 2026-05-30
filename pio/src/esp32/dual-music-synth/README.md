# JotCAD ESP32 Dual Music Synthesizer Node

> [!IMPORTANT]
> **HARDWARE COMPATIBILITY NOTICE**:
> *   **Supported Hardware**: Classic original dual-core ESP32 microcontrollers (e.g., **ESP32-WROOM-32**, **ESP32-WROVER**, or **ESP32-DevKitC**).
> *   **Unsupported Hardware**: **ESP32-S3**, **ESP32-S2**, **ESP32-C3**, and **ESP32-C6** are **NOT supported**. While some possess dual-core CPUs (such as the S3), they lack the physical **Classic Bluetooth (BR/EDR)** radio hardware needed to transmit standard high-fidelity audio streams over **A2DP** (they are restricted to Bluetooth Low Energy only).

This directory contains the firmware for the **JotCAD ESP32 Dual Music Synthesizer Node** (`esp32_dual_music_synth`), a dedicated high-fidelity wireless music synthesizer targeting classic dual-core ESP32 microcontrollers. 

This node functions entirely without wires: it connects asynchronously as a **BLE-MIDI Client** to your `SMK25Mini` keyboard, maps key velocity to polyphonic voice channels, synthesizes real-time stereo CD-quality audio (44.1kHz) in memory, and broadcasts the audio to **Bluetooth Earphones/Speakers** via Classic Bluetooth **A2DP Source**.

---

## 1. Technical Architecture

Running real-time high-fidelity sound synthesis concurrently with two active wireless protocols (BLE and Classic Bluetooth) is extremely resource-intensive. To prevent buffer underruns, audio stuttering, and connection dropouts, the system utilizes a strict **Dual-Core scheduling allocation** and **Wi-Fi radio bypass**:

```mermaid
graph TD
    subgraph Core 1 (Network & Control Thread)
        BLE[BLE-MIDI Client] -->|Midi Events| VM[Polyphonic Voice Mapper]
        VM -->|Note On/Off| EN[ADSR Envelopes]
        HB[10s Reconnection Heartbeat] -->|Self-Healing| BLE
    end

    subgraph Core 0 (A2DP Audio Thread)
        A2DP[A2DP Source Streaming] -->|PCM Audio Pull Callback| SC[Sound Data Callback]
        SC -->|Render Demand| ES[ESP32Synth Pull Generator]
        EN -->|Wave & Frequency State| ES
    end

    subgraph Bluetooth Earphones
        A2DP -->|Wireless Stereo SBC stream| EP[Earphones / Speaker]
    end

    subgraph Keyboard
        KB[SMK25Mini Keyboard] -->|Wireless BLE-MIDI| BLE
    end
    
    style Core 1 fill:#2b2d42,stroke:#8d99ae,stroke-width:2px,color:#fff
    style Core 0 fill:#1d3557,stroke:#457b9d,stroke-width:2px,color:#fff
    style EP fill:#003049,stroke:#d62828,stroke-width:2px,color:#fff
    style KB fill:#003049,stroke:#fcbf49,stroke-width:2px,color:#fff
```

### Core Allocation Breakdown:
*   **Core 1 (Control Plane)**: Manages incoming wireless BLE-MIDI packets from the keyboard, performs active device scans, parses note-on/note-off actions, executes the 16-note round-robin polyphonic voice mapping, and runs the 10-second automatic reconnection heartbeat.
*   **Core 0 (Audio Plane)**: Dedicated exclusively to high-priority audio streaming. The Bluetooth A2DP stack fires high-frequency buffer callbacks to stream sound data. Inside the callback, the synthesizer generates custom stereo PCM samples at exactly 44.1kHz.
*   **Antenna Coexistence**: Standard Wi-Fi mesh routing (`ENABLE_VFS`) is **temporarily disabled** on this node to give the 2.4 GHz radio controller maximum capacity, eliminating RF band contention between BLE and A2DP.

---

## 2. Compilation Compatibility Bridge

The `ESP32Synth` library utilizes modern **ESP-IDF v5.x channel-based I2S APIs**, while PlatformIO compiles classic ESP32 environments against **ESP-IDF v4.x legacy monolithic APIs**. To resolve this major framework collision, we designed a zero-overhead compatibility bridge in `ESP32Synth`:

1.  **Header Guards (`__has_include`)**: We wrapped v5.x specific headers like `driver/i2s_std.h`, `driver/i2s_pdm.h`, and `driver/dac_continuous.h` inside robust `#if __has_include` preprocessor checks, preventing compiler crashes on legacy platforms.
2.  **Type Stubbing**: Unresolved v5.x handles (e.g. `i2s_chan_handle_t` and `dac_continuous_handle_t`) are stubbed as standard generic `void*` pointers on legacy builds, ensuring the class definition is structurally valid.
3.  **Cycle Count Re-mapping**: ESP-IDF v5.x renamed CPU performance cycle counting APIs. We mapped `esp_cpu_get_cycle_count` directly to the legacy `esp_cpu_get_ccount` macro on classic ESP32 targets, resolving all mathematical profiling blocks with zero runtime overhead:
    ```cpp
    #if defined(CONFIG_IDF_TARGET_ESP32)
    #define esp_cpu_get_cycle_count esp_cpu_get_ccount
    #endif
    ```
4.  **A2DP Macro Translation**: The `ESP32-A2DP` library depends on an audio state enum value (`ESP_A2D_AUDIO_STATE_SUSPEND`) introduced in ESP-IDF v5.x. We injected a compiler mapping flag inside `platformio.ini` to cleanly translate this to the legacy enum name without modifying the library source code:
    ```ini
    build_flags = 
        -D ESP_A2D_AUDIO_STATE_SUSPEND=ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND
    ```

---

## 3. Polyphony & Synthesizer Settings

The synthesizer is configured with a rich, clean-sounding default patch:
*   **Polyphony**: Up to 16 concurrent voices using a thread-safe round-robin allocation.
*   **Oscillator Waveform**: Clean, warm **Triangle waves** to minimize harmonic distortion and emulate analog synthesizers.
*   **Envelope (ADSR)**:
    *   **Attack**: 20ms (responsive, snappy onset).
    *   **Decay**: 150ms.
    *   **Sustain**: 75% level (`192` out of 255).
    *   **Release**: 250ms (smooth, natural note decay after key release).
*   **Master Volume**: Set to maximum 8-bit level (`255`) to provide a rich, loud signal without distortion.

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
[DIAGNOSTIC] Wi-Fi: DISCONNECTED | BLE Keyboard: CONNECTED | Earphones: CONNECTED | Last Event: {"type":"note_on","channel":0,"note":60,"velocity":100,"source":"ble"}
```

*   **Self-Healing Reconnection**: If the `SMK25Mini` keyboard is turned off or goes out of range, the background scanner detects the loss of connection and will automatically trigger active background scans every 10 seconds to re-establish the connection.
*   **A2DP Connection**: The Classic Bluetooth stack manages automatic re-pairing with the last connected Bluetooth earphones/speakers when they enter pairing mode.

---

## 6. Critical Multitasking & API Rules

### A. Prevent CPU Starvation (Mandatory loop Yields)
On dual-core ESP32 microcontrollers, the primary application loop (`loop()`) executes on Core 1 by default. When VFS/Wi-Fi is disabled, a standard loop check containing no delay runs as a tight infinite block.
*   **The Bug**: Consumes 100% of Core 1's CPU capacity, completely starving lower-priority background tasks on Core 1 (such as the Bluedroid stack, A2DP state machine handshakes, and FreeRTOS connection timers). This blocks the A2DP stream from ever starting.
*   **The Rule**: **Always** place a short yield or delay (e.g., `delay(10);` or `vTaskDelay(pdMS_TO_TICKS(10));`) at the end of the `loop()` function. This immediately releases CPU control and permits the background Bluetooth stack to complete handshakes seamlessly.

### B. Standard 8-Bit Volume Scaling (Avoid Bitwise Overflow)
The `ESP32Synth` library is designed to accept voice and master volume inputs in an **8-bit range (0 to 255)**.
*   **The Bug**: Internally, `noteOn()` and `setVolume()` shift the volume left by 8 bits (`volume << 8`) to fit into a 16-bit unsigned voice container (`uint16_t vol`). Scaling MIDI note velocity to a 16-bit range (e.g., `velocity * 512 = 51200`) causes a bitwise overflow when shifted left by another 8 bits inside a `uint16_t` ($51200 \times 256 \equiv 0 \pmod{65536}$), reducing the note volume to **exactly 0 (complete silence)**.
*   **The Rule**: Standardize all volume inputs (both per-voice and master) to a standard **8-bit range (0 to 255)**. Scale standard 7-bit MIDI velocity (0-127) using a simple left-shift: `(velocity > 127) ? 255 : (velocity << 1)`.

