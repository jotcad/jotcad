# JotCAD ESP32 MIDI Reader VFS Node

This directory contains the application firmware for the **JotCAD ESP32 MIDI Reader VFS Node**.

## Responsibility

This application node runs on an ESP32 or ESP32-C3 development board and performs the following tasks:
1. **Concurrent Wireless Access**: Hosts both a Bluetooth Low Energy (BLE) MIDI server stack and standard Wi-Fi station connectivity to communicate with the JotCAD Virtual File System (VFS) mesh network.
2. **BLE-MIDI Server**: Advertises as `JotCAD MIDI Reader`, accepting incoming connections from digital audio workstations (DAWs), controllers, or mobile devices, parsing incoming `Note On`, `Note Off`, and `Control Change` events asynchronously.
3. **Hardware Serial MIDI Interface**: Processes physical MIDI input dynamically via hardware UART pins as a legacy fallback. It is dynamically remapped at compile-time to match the microcontroller's hardware capabilities:
   * **Classic ESP32 (Xtensa)**: Uses `Serial2` (RX=16, TX=17) at standard MIDI speeds (31250 baud).
   * **ESP32-C3 (RISC-V)**: Uses `Serial1` (RX=4, TX=5) at 31250 baud (since `Serial2` does not exist on single-core ESP32-C3 chips).
4. **VFS Mesh Broadcast**: Instantly packages any incoming BLE or Serial MIDI event into JSON format and publishes/broadcasts it over the VFS mesh via `/midi/events`.
5. **VFS RPC Endpoint**: Hosts `/midi/status` RPC queries, allowing peer nodes to pull the last received MIDI event thread-safely.

## File Summary

- [main.cpp](file:///home/brian/github/jotcad/pio/src/esp32/midi-reader/main.cpp): Core application lifecycle setup, callback configurations, thread-safe memory management, serial parser state machines, and main FreeRTOS concurrent scheduler ticks.

---

## Flashing & Device Discovery Guide

### 1. Identifying Connected Devices

Before compiling and flashing, you can query which serial/USB devices are currently connected to your host machine by executing the PlatformIO device list command:

```bash
# List all active serial/COM ports and USB bridges
pio device list
```

#### Connected Device Targets:
* **`/dev/ttyACM0` (or `/dev/ttyACM*`)**:
  * **Hardware ID**: `USB VID:PID=303A:1001`
  * **Description**: `USB JTAG/serial debug unit` (Connected native USB OTG JTAG interface of your ESP32-C3 board).

Alternatively, you can query these ports natively on Linux using:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```

### 2. Flashing the Firmware

To compile, flash, and immediately stream the serial logging monitor, run the provided flashing script from the root `pio` directory:

```bash
# Compile and flash to your ESP32-C3 via /dev/ttyACM0
./flash_esp32dev_midi_reader.sh 1
```

*(The optional `1` suffix parameter appends `-01` to your VFS Node ID for standard identification: e.g. `esp32-midi-reader-01`)*.

### 3. Targeting a Specific USB Port

If you have multiple devices connected and want to force PlatformIO to target a specific serial port (e.g. `/dev/ttyUSB0` or `/dev/ttyACM0`), you can do so in two ways:

#### A. Command Line Port Override
You can override the upload and monitoring port directly when running PlatformIO commands:
```bash
# Upload to a specific port
pio run -e esp32_midi_reader -t upload --upload-port /dev/ttyACM0

# Monitor a specific port
pio run -e esp32_midi_reader -t monitor --monitor-port /dev/ttyACM0
```

#### B. PlatformIO Configuration Lock
Open `pio/platformio.ini` and append target overrides to the `[env:esp32_midi_reader]` block:
```ini
[env:esp32_midi_reader]
...
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
```

---

## Logging & Runtime Diagnostics

The application utilizes highly verbose, real-time serial logging running at **`115200`** baud to help trace system health. 

### Bootstrapping Logs
When the device powers on, it prints status indicators confirming hardware and network configurations:
```text
[ESP32] JotCAD VFS MIDI Reader Node Booting...
[Wi-Fi] Connecting to SSID: 'Your_SSID'...
[Wi-Fi] Connected successfully!
[Wi-Fi] ESP32 Local IP: 192.168.1.142
[BLE-MIDI] Initializing BLE-MIDI stack...
[BLE-MIDI] Advertising device name 'JotCAD MIDI Reader' (Just Works pairing).
[SERIAL-MIDI] Listening on Hardware Serial1 (RX=4, TX=5) at 31250 baud.
[VFS] Bootstrapping VFS Client Node...
[VFS] Probing neighbor router http://192.168.1.100:8080/health...
[VFS] Gateway Health Check: SUCCESS (Status Code 200)
[VFS] Node fully active! Registering VFS_READY signal.
VFS_READY
```

### Event Diagnostics Logs
Whenever a MIDI controller or DAW transmits notes to the ESP32 via Bluetooth or physical serial cables, the system broadcasts the message and prints immediate diagnostic outputs:
```text
# Bluetooth Note Press
[BLE-MIDI] NOTE ON - Channel: 1, Note: 60, Velocity: 100 (Timestamp: 4820)
[VFS] Broadcasting event to mesh: {"channel":1,"note":60,"source":"ble","timestamp":4820,"type":"note_on","velocity":100}
[VFS] Broadcast completed. Sent to 1 peer(s).

# Bluetooth Note Release
[BLE-MIDI] NOTE OFF - Channel: 1, Note: 60, Velocity: 0 (Timestamp: 5120)
[VFS] Broadcasting event to mesh: {"channel":1,"note":60,"source":"ble","timestamp":5120,"type":"note_off","velocity":0}
[VFS] Broadcast completed. Sent to 1 peer(s).

# Hardware UART MIDI stream
[SERIAL-MIDI] Note On  - Ch:1, Note:64, Vel:90
[VFS] Broadcasting event to mesh: {"channel":1,"note":64,"source":"serial","type":"note_on","velocity":90}
[VFS] Broadcast completed. Sent to 1 peer(s).
```

### 10-Second Keep-Alive Heartbeat
Every 10 seconds, the device dumps its local wireless network status and the details of the last parsed MIDI event thread-safely:
```text
[DIAGNOSTIC] Wi-Fi Status: CONNECTED | BLE Connected: YES | Last Event: {"channel":1,"note":60,"source":"ble","timestamp":5120,"type":"note_off","velocity":0}
```
