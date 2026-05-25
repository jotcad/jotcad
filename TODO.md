# Project TODO List

## Compiler & Language
- [ ] **Fix `JotNumbersConsumer` Plural Consumption**: Update the compiler to allow operators expecting `jot:numbers` to consume Selectors that return plural number types (e.g., generator calls like `range`). Currently, the consumer only accepts raw numbers or literal arrays.
- [ ] **Fix `JotStringsConsumer` Plural Consumption**: Apply similar logic to strings for consistency across all plural types.

## Geometry Kernels
- [ ] **Standardize Plural Producers**: Audit C++ operators to ensure they correctly declare `jot:numbers` or `jot:strings` in their output schema if they return sequences.

## Embedded (ESP32)
- [x] **Initialize PlatformIO project**: Configured for ESP32 with C++17 and AsyncWebServer.
- [x] **Setup Simulation**: Verified local Wokwi simulation pipeline with `npm run test:pio`.
- [ ] **Port VFS Core**: Implement `VFSNode` and `CID` logic for the ESP32 environment.
- [ ] **Async Networking**: Implement mesh-compatible HTTP/WebSocket handlers using ESPAsyncWebServer.
- [ ] **Flash Storage**: Implement persistent artifact storage using SPIFFS or LittleFS.
