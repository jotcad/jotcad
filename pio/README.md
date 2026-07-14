# PlatformIO (PIO) Embedded Mesh Nodes

This directory contains the PlatformIO project for JotCAD's embedded VFS/Zenoh mesh nodes (ESP32 and ESP8266), enabling microcontrollers to publish sensor data and subscribe to actuator target controls directly over the decentralized mesh.

## Responsibilities
- Embedded C++ firmware interfacing with physical actuators (e.g., 28BYJ-48 stepper motors) and sensors.
- Local VFS (Virtual File System) client nodes built on the `zenoh-pico` library.
- Automated USB/OTA flashing and serial monitoring utilities.

## Contents
- **`src/`**: Node firmware implementations (e.g., `esp8266/stepper_node/`).
- **`include/`**: Common embedded headers (VFS interface class).
- **`flash_*.sh`**: Compilation and flashing helper scripts.
- **`monitor_*.sh`**: Serial logging monitors.
- **`patch_zenoh_pico.py`**: Automated build script to patch library transport limits and bugs.

---

## ESP8266 & Zenoh-Pico Single-Threaded Handshake Patch

### The Problem (`z_open returned: -99`)
Unlike the dual-core, multi-threaded ESP32 (which runs FreeRTOS with `Z_FEATURE_MULTI_THREAD=1`), the ESP8266 operates in a single-threaded runtime environment (`Z_FEATURE_MULTI_THREAD=0`).

In single-threaded mode, `zenoh-pico`'s default Arduino/OpenCR transport implementation (`tcp_opencr.cpp`) has a critical bug in `_z_tcp_opencr_read_exact`:
- During the `z_open` handshake, it attempts to read the incoming `InitAck` packet.
- Because the network packet might not arrive at the exact microsecond the read is called, the hardware buffer's `available()` returns `0`.
- The library's original `_z_tcp_opencr_read_exact` broke the loop and returned `0` immediately on `available() == 0`, interpreting it as an immediate connection failure.
- This aborted the handshake, causing `z_open` to consistently fail on single-threaded hardware with error `-99` (`_Z_ERR_TRANSPORT_RX_FAILED`).

### The Solution (Cooperative Polling Read)
Our build system hooks into the PlatformIO library compilation process using `pio/patch_zenoh_pico.py` to rewrite the socket read layer:
1. **Non-Blocking Signaling**: `_z_tcp_opencr_read` returns `SIZE_MAX` (acting as `EWOULDBLOCK` / `EAGAIN`) when the connection is healthy but no bytes are currently available in the hardware buffer.
2. **Cooperative Blocking Read**: `_z_tcp_opencr_read_exact` uses a loop that delays for `1ms` using the ESP8266's core `delay(1)` (yielding control to keep the WiFi stack and watchdog timer fed) if `available()` is 0, until either the connection is dropped or all requested bytes are successfully read.

This fix achieves a 100% reliable connection handshake while keeping the ESP8266's single thread lightweight (using ~7.3 KB of heap space during VFS operation).

---

## Getting Started: stepper-motor-01 (ESP8266)

The stepper node controls a 28BYJ-48 stepper motor driven by a ULN2003 driver board, subscribing to target updates over the VFS mesh.

### Hardware Connection (ULN2003 -> WeMos D1 Mini)
* **IN1** -> GPIO 14 (D5)
* **IN2** -> GPIO 12 (D6)
* **IN3** -> GPIO 13 (D7)
* **IN4** -> GPIO 15 (D8)
* **VCC** -> 5V (external power recommended)
* **GND** -> GND (common ground)

### 1. Compile & Flash
Make sure the ESP8266 is plugged in and recognized (typically `/dev/ttyUSB0`):
```bash
./pio/flash_esp8266_stepper.sh
```
This script automatically runs the `patch_zenoh_pico.py` script, compiles the firmware with the corrected read/write loop, uploads it, and initiates a serial monitor.

### 2. Verify Connection
Upon booting, the serial console should report:
```text
[ESP8266-Stepper] WiFi Connected.
[ESP8266-Stepper] IP Address: 192.168.0.12
[VFS esp8266-stepper-EAB555] Connecting to Zenoh locator: tcp/192.168.0.14:9000
[VFS] Zenoh session opened successfully.
[VFS] Subscribing to notification topic: jot/vfs/pub/actuator/stepper/target
[ESP8266-Stepper] VFS Node Ready.
VFS_READY
```

### 3. Send Stepper Commands
Send rotation targets to the VFS mesh:
```bash
# Rotate +180 degrees (clockwise) relative to current position
node scratch/test_publish_stepper.js --angle 180

# Rotate -90 degrees (counter-clockwise)
node scratch/test_publish_stepper.js --angle -90
```
The node will step the motor to the target angle, process VFS session keep-alives continuously in the background, and publish its final state update to `jot/vfs/pub/actuator/stepper/state` when movement completes.
