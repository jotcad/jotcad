# ESP32 Dual Music Synthesizer & MIDI Specification

This document details the configuration interface, VFS mesh protocols, and audio engine modes of the JotCAD ESP32 Synthesizer node (`esp32_dual_music_synth`).

---

## 1. VFS Mesh Routes & Dynamic Configuration

The synthesizer configuration is driven by the Virtual File System (VFS) and synchronized dynamically over the local WebSocket mesh.

### Configuration Path: `instrument/config`
*   **Description**: Retrieves the active set of dynamic instruments.
*   **Format**: JSON payload containing an array of instrument structures.
*   **Active Sync**: The ESP32 node issues a pull request for this Selector on boot and registers a local notification listener (`register_notify_handler`). Whenever a new configuration is published from the UX node, the ESP32 node dynamically hot-swaps active voice profiles in real time.

#### Instrument Structure Schema:
```json
{
  "name": "Instrument Name",
  "wave": -2,
  "pulse_width": 128,
  "attack": 10,
  "decay": 1500,
  "sustain": 0,
  "release": 250,
  "wavetable_id": 0,
  "sample_id": 0,
  "stream_id": 0,
  "volume_scale": 240
}
```

### MIDI Play Path: `synth/note`
*   **Description**: Receives play/release actions from the mesh (e.g., virtual keyboard) and triggers local synthesizer voices.
*   **Parameters**:
    *   `note`: MIDI note number (0-127).
    *   `velocity`: Velocity value (0-127) for volume calculations.
    *   `on`: Boolean indicating whether to trigger note-on (`true`) or note-off (`false`).

### Remote Selection Path: `synth/control`
*   **Description**: Receives remote instrument index change signals from the UX control panel.
*   **Parameters**:
    *   `control`: CC control index (typically `7` for active instrument selection).
    *   `value`: Selector value mapping directly to the active instrument index.

---

## 2. Audio Generator Engines (The 5 Wave Types)

The ESP32 dual-core synth supports five distinct sound generation engines, set via the `wave` parameter in the instrument configuration:

### Type 1: Basic Oscillators (`wave < 0`)
Generates classic analog synthesizer waveforms:
*   **Sine (`-1`)**: Clean, fundamental-dominated tone. Excellent for flutes, bells, and pure pads.
*   **Triangle (`-2`)**: Warm analog-style waveform with odd harmonics. Default for acoustic instruments like pianos and harps.
*   **Sawtooth (`-3`)**: Rich, bright harmonic series. Perfect for thick leads, brass, and dense string ensembles.
*   **Pulse (`-4`)**: Rectangular waveform. The duty cycle is controlled by the `pulse_width` parameter (0-255, where `128` is a symmetrical 50% square wave). Ideal for retro chiptune sounds and hollow clarinets.
*   **Noise (`-5`)**: Pseudo-random white noise generator. Perfect for percussion elements, atmospheric FX, seashore sounds, or wind.

### Type 2: Wavetables (`wave = 1`)
*   **Description**: Uses pre-calculated, cyclical digital waveforms loaded into memory.
*   **Configuration**: Matches `wavetable_id` to retrieve registered waveforms on setup (e.g., `buzzy_wavetable` and `hollow_wavetable`).
*   **Usage**: Enables complex digital textures, bell-like chimes, and complex wavetable sweeps.

### Type 3: Samples (`wave = 2`)
*   **Description**: Plays back short, pre-recorded PCM audio samples stored in flash.
*   **Configuration**: Matches `sample_id` to select active sample entries.
*   **Usage**: Designed for realistic percussion hits, acoustic drum kits, or short one-shot sound effects.

### Type 4: Streams (`wave = 3`)
*   **Description**: Decodes and streams continuous, long-running audio files.
*   **Configuration**: Matches `stream_id` to route streaming audio data onto specific voice tracks.
*   **Usage**: Ideal for ambient loops, backing tracks, or complex background voice streams.

### Type 5: Custom Wave Callbacks (`wave = 4`)
*   **Description**: Executes custom C++ math algorithms in the high-priority core-0 render loop.
*   **Implementation**: Utilizes an FM synthesis algorithm (`fm_custom_wave`) using a two-operator carrier-modulator configuration. The phase of the carrier is modulated in real time by a modulator sine wave to create metallic, frequency-modulated textures.

---

## 3. Volume Scaling & Leveling

Different synthesis engines generate sound at vastly different perceived loudnesses (e.g., a full amplitude sawtooth or noise wave sounds significantly louder than a clean sine or triangle wave).

To balance this, each instrument features a **`volume_scale`** byte parameter (`0` to `255`):
*   **Integration**: When a note is played, its raw velocity is multiplied by the instrument's `volume_scale` before being mapped to the polyphonic voice envelope level:
    $$\text{Final Amplitude} = \frac{\text{Velocity} \times \text{volume\_scale}}{127}$$
*   **Default Settings**: High-output oscillators (like Sawtooth and Pulse) are scaled lower (e.g., `110`-`120`), whereas low-output waves (like Sines and Triangles) are scaled higher (e.g., `240`-`255`) to maintain a balanced, pleasant listening experience across presets.

---

## 4. MIDI CC 7 Selector Mapping

*   **Non-Scaled Indexing**: Control Change **CC 7** is used to select the active instrument.
*   **Behavior**: Unlike standard scaled volume mappings, CC 7 values (`0` to `127`) represent the **direct instrument index** in the loaded configuration list.
*   **Capping**: If the CC 7 value exceeds the number of available instruments, it automatically caps at the highest valid index (size - 1). This ensures that standard controllers can query any of the 128 General MIDI instrument presets directly.
