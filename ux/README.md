# JotCAD Spatial Desktop

A professional-grade "Infinite Blackboard" operating system for geometric
modeling.

## Architecture

The JotCAD UX follows a "Spatial Desktop" metaphor, separating the world-space
canvas from screen-space application windows.

-   `src/components/system/desktop/`: The core Desktop environment, including
    the Window Manager and Desktop Icons.
-   `src/components/canvas/`: The infinite pannable and zoomable blackboard.
-   `src/components/editor/`: Windowed code editors and result viewports.
-   `src/components/discovery/`: Mesh mapping apps and schema catalogs.
-   `src/lib/vfs/Worksheet.js`: The unified persistence layer (LocalStorage +
    RemoteStorage) with 3-way merging.

## Interaction Model

### The Blackboard
-   **Desktop**: Hold **Shift**, **Alt**, or **Meta** to enter "World Mode."
    Left-click drag anywhere to pan; scroll wheel anywhere to zoom.
-   **Mobile**: Two-finger pinch to zoom and pan.
-   **Middle Mouse**: Global pan at any time (CAD standard).

### Application Windows
-   **Maximized Mode**: Tapping the expand button "portals" the window to the
    document body, bypassing blackboard zoom and ensuring **1:1 logical pixel
    parity** (DPI compensation) for perfect legibility on mobile.
-   **Tap Targets**: All interactive elements in maximized windows enforce a
    minimum **44x44px** hitbox.

## Unified Persistence (Worksheet)

The `Worksheet` abstraction handles cross-device synchronization of:
1.  **JOT Source Code** (Text-based `diff3` merge).
2.  **Desktop Layout** (Spatial icon positions via `trimerge`).
3.  **Window Session** (Which apps are open and where).
4.  **Assets** (Icon images and custom tool data).

## CID Validation

### 1. Start the Full System

The easiest way to start the Hub, Dispatcher, and UX simultaneously is from the
root directory:

```bash
npm start
```

### 2. Manual Start (Individual Components)

If you need to run components separately:

#### Start the VFS Hub

```bash
node fs/serve.js
```

#### Start the Dispatcher (for C++ Agents)

```bash
# In the root or geo directory
node orchestrator.js # (Or use the inline dispatcher script from orchestrator.js)
```

#### Start the UX App

```bash
cd ux
npm run dev -- --port 3030
```

Visit `http://localhost:3030` to interact with the blackboard.

## CID Validation

The UX performs strict validation of Content-IDs (CIDs) received from the Hub.
If a `CID Mismatch` error appears in the console, it indicates a hashing
discrepancy between the Browser and the Hub.
