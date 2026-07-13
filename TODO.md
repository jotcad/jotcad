# Project TODO List

## Compiler & Language
- [x] **Fix Selector Injection in Typed Consumers**: Update `JotNumbersConsumer`, `JotStringsConsumer`, and others to allow "mixed" pools. Currently, these consumers perform eager type-checking on evaluated values and fail if they encounter a `Selector` (e.g., from `range()`) instead of a raw primitive. They should instead:
    - Recognize `Selector` objects as valid "recipes" for the expected type.
    - Check the Selector's output port type using `_getSelectorOutputType`.
    - Allow the VFS to handle the final flattening/resolution.
- [x] **Refactor `_isJotNumber/String` for Late-Binding**: These guards currently return `false` for Selectors. They need to be updated to support symbolic or selector-based identity.

## Geometry Kernels
- [x] **Standardize Plural Producers**: Audit C++ operators and ensure metadata correctly declares `jot:numbers` or `jot:strings` in their output schema. (Hardened via `Processor::decode` automatic pluralization).
- [x] **Verify `Simplify` Operator**: Resolved stalling issue (dihedral unit mismatch) and integrated `geo/test/simplify_test.cpp` to verify topological integrity and volume stability.
- [ ] **Upgrade CGAL Library to 6.2+**: Update the `pentacular/cgal` git clone to pull the latest 2026 commits. This will vendor the new `approximate_convex_decomposition` package (`<CGAL/approximate_convex_decomposition.h>`) into `geo/cgal/` to enable fast approximate 3D convex decomposition.

## Rendering & Visuals
- [x] **Visual Regression for Translucency**: Implemented `integration/puppeteer/translucency.test.js` and fixed `GeometryDecoder.js` to correctly honor the `ghost` role and `opacity` tags. Verified translucent overlap in standard cluster.
- [ ] **Hitchhiker Grid Alignment**: Implement a screen-aligned, scale-invariant overlay grid that subdivides on zoom (HUD-style).
    - [ ] Coordinate Sync: Ensure `[0,0,0]` alignment during camera pan/zoom.
    - [ ] Dynamic Subdivision: Line density adjusts based on zoom power-of-10.
    - [ ] Orientation Cues: Visual indicator for workplane alignment (XY/XZ/YZ).

## CAD Features & BOM
- [x] **Conformal Wrapping**: Implemented `jot/conform` for wrapping geometry onto curved surfaces.
- [x] **BOM Tagging Convention**: Standardized `provenance` (unique) and `sku` (non-unique) tags for physical stock tracking.
- [ ] **BOM Report Generator**: Implement a JS-side utility to traverse a shape hierarchy and generate a formatted CSV/JSON Bill of Materials.

## Embedded (ESP32)
- [x] **Initialize PlatformIO project**: Configured for ESP32 with C++17 and WebSockets.
- [x] **Setup Simulation**: Verified local Wokwi simulation pipeline with `npm run test:pio`.
- [ ] **Port VFS Core (DEFERRED)**: Implement `VFSNode` and `CID` logic for the ESP32 environment.
- [ ] **Async Networking (DEFERRED)**: Implement mesh-compatible WebSocket handlers using `WebSockets` library.
- [ ] **Flash Storage (DEFERRED)**: Implement persistent artifact storage using SPIFFS or LittleFS.

## VFS & Mesh (Zenoh Migration)
- [x] **Clean Up Rubbish / Legacy Compatibility Stubs**: Refactor `VFSManager.js` and visualizer components (like `MeshGraphApp.jsx`) to remove dependencies on legacy WebSocket tunnel maps (`mesh.peers` and `mesh.interests`), aligning them fully with Zenoh's native pub-sub topology and metadata queries. Clean up the placeholder dummy maps in `MeshLinkBase`.
