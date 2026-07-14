# JotCAD UX Frontend Source

This directory contains the SolidJS web application source code for the JotCAD user interface, incorporating real-time WebGL rendering and distributed VFS state synchronization.

## Directory Structure

* [components/](file:///home/brian/github/jotcad_ez/ux/src/components) — Modular UI components:
  * `viewport/`: Three.js WebGL canvas wrapper, handling mouse/touch navigation, dynamic overlays, selection highlights, and grid rendering.
  * `system/`: Dashboard widgets for system telemetry, climate logs, MIDI synths, and webcam feeds.
  * `discovery/`: Graph and tree visualizers mapping the active Zenoh mesh topology and provider nodes.
* [lib/](file:///home/brian/github/jotcad_ez/ux/src/lib) — Core logical client state:
  * `VFSManager.js`: Client-side virtual file system coordinator, draining streams and updating the Solid store.
  * `blackboard.js`: Registry for tracking shape formulas, computed mesh geometries, and parameter inputs.
  * `render/`: Mesh decoding utilities (`GeometryDecoder.js`) converting binary arrays into WebGL-compatible geometries.

## Main Entry Files

* `index.jsx` — Bootstraps the SolidJS application, configures the client mesh transport connection, and renders the desktop shell.
* `index.css` — Global stylesheets containing color tokens, responsive layout rules, and custom scrollbar overrides.
