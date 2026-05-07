# JotCAD Interactive UX

A high-performance reactive graph visualizer for the JotCAD Distributed
Blackboard.

## Architecture

The UX is organized into atomic modules to maintain high maintainability and
precise responsibility:

-   `src/components/canvas/`: The spatial blackboard layer and its nodes.
-   `src/components/editor/`: Code editors and result viewports.
-   `src/components/discovery/`: Mesh mapping and schema catalogs.
-   `src/components/system/`: HUD elements (Console, Error Overlays).
-   `src/lib/render/`: Shared WebGL renderer and JOT-to-Three.js decoders.
-   `src/lib/state/`: Reactive application and mesh state.
-   `src/lib/vfs/`: VFS/Mesh connection and bridge logic.

## Interaction Model

### Unified Gestures
The blackboard uses a custom, non-passive gesture system to ensure smooth
panning and zooming on both touch and mouse devices.

-   **Two-Finger Pinch/Pan**: Zooms relative to the gesture center and pans
    simultaneously.
-   **Gesture Ownership**: Multi-touch gestures are strictly "owned" by either
    the blackboard or a specific window to prevent accidental window raising or
    dragging during navigation.
-   **Manual Viewports**: 3D viewports are static snapshots by default and
    become "Active" only when focused, ensuring 60fps performance even with
    hundreds of windows.

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
