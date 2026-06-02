# Project TODO List

## Compiler & Language
- [ ] **Fix Selector Injection in Typed Consumers**: Update `JotNumbersConsumer`, `JotStringsConsumer`, and others to allow "mixed" pools. Currently, these consumers perform eager type-checking on evaluated values and fail if they encounter a `Selector` (e.g., from `range()`) instead of a raw primitive. They should instead:
    - Recognize `Selector` objects as valid "recipes" for the expected type.
    - Check the Selector's output port type using `_getSelectorOutputType`.
    - Allow the VFS to handle the final flattening/resolution.
- [ ] **Refactor `_isJotNumber/String` for Late-Binding**: These guards currently return `false` for Selectors. They need to be updated to support symbolic or selector-based identity.

## Geometry Kernels
- [ ] **Standardize Plural Producers**: Audit C++ operators to ensure they correctly declare `jot:numbers` or `jot:strings` in their output schema if they return sequences.
- [ ] **Verify `Simplify` Operator**: Create `geo/test/simplify_test.cpp` to verify topological integrity, volume stability, and sharp edge preservation under various reduction ratios.

## Rendering & Visuals
- [ ] **Visual Regression for Translucency**: Create a Puppeteer or PNG-based test that explicitly verifies alpha blending. It should check that back-to-front sorting works and that the Z-buffer isn't blocking fragments with alpha < 255.
- [ ] **Hitchhiker Grid Alignment**: Verify the overlay grid's scale and persistence as a reference in the WebGL renderer.

## Embedded (ESP32)
- [x] **Initialize PlatformIO project**: Configured for ESP32 with C++17 and AsyncWebServer.
- [x] **Setup Simulation**: Verified local Wokwi simulation pipeline with `npm run test:pio`.
- [ ] **Port VFS Core**: Implement `VFSNode` and `CID` logic for the ESP32 environment.
- [ ] **Async Networking**: Implement mesh-compatible HTTP/WebSocket handlers using ESPAsyncWebServer.
- [ ] **Flash Storage**: Implement persistent artifact storage using SPIFFS or LittleFS.
