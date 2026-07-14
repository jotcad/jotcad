# VFS & Mesh JavaScript Implementation Source

This directory contains the JavaScript and Node.js implementation of the Virtual File System (VFS) and the Zenoh-based decentralized mesh networking layer.

## Key Subdirectories

* [vfs/](file:///home/brian/github/jotcad_ez/fs/src/vfs) — Core virtual file system logic, including:
  * `vfs.js`: Standard VFS interface supporting provider registrations, schema lookups, and event dispatch.
  * `resolver.js`: Resolution algorithm containing recursive link resolution, local-only safety guards, and remote query fallbacks.
  * `schema.js`: Schema structure verification.
* [mesh/](file:///home/brian/github/jotcad_ez/fs/src/mesh) — Peer-to-peer and bridge communication:
  * `mesh_link.js`: Main Zenoh client coordinator. Subscribes to metric updates, manages remote routing pathways, and publishes active provider schemas.
  * `reverse_connection.js` / `forward_connection.js`: TCP-like tunneling layer built on WebSocket bridges.

## Key Files

* `cid.js` — Handles Content Addressable Identification. Implements **JotCAD Canonical Binary (JCB)** encoding and Base64-JCB translations to produce stable, deterministic hashes (CIDs).
* `vfs_core.js` — Core environment-agnostic VFS module exports.
* `vfs_browser.js` — Web-browser VFS flavor, backed by `IndexedDBStorage` for caching geometries and thumbnails locally.
* `vfs_node.js` — Server/daemon VFS flavor, backed by `DiskStorage` (`.data` and `.meta` files) on the host filesystem.
* `vfs_rest_server.js` — REST integration API server exposing routes (`/read_selector`, `/read_cid`, `/write`) to remote processes.
