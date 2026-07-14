# JotCAD Virtual File System (VFS) & Mesh Networking

This directory contains the Virtual File System (VFS) and mesh networking implementation for JotCAD, bridging C++ compilation nodes, WebGL visualizers, and decentralized peer nodes.

## Directory Structure

* [cpp/](file:///home/brian/github/jotcad_ez/fs/cpp) — Core C++ VFS library. Integrates with OpenSSL (`-lcrypto`, `-lssl`) and the Zenoh-C client library to provide content hashing and distributed query resolution.
* [src/](file:///home/brian/github/jotcad_ez/fs/src) — Core JavaScript/Node.js implementation of the VFS, including Zenoh WebSocket client transport and network discovery topology loops.
* [test/](file:///home/brian/github/jotcad_ez/fs/test) — Test suite containing VFS storage compliance checks, resolution loop tests, and network pub-sub integration runners.

## Architecture Overview

The VFS implements a hybrid Content-Addressable Storage (CAS) and Computational recipe routing layer:

1. **Content Addressing (CID)**: Geometry data blocks are hashed cryptographically using SHA-256 over their canonical serialization. CIDs represent immutable content.
2. **Computational Recipes (Selectors)**: Recipes are structural operations (JSON definitions of functions and arguments). Hashing a standardized Selector JSON produces a stable identity CID representing the computation.
3. **Recursive Resolution**: When requesting a Selector:
   * The VFS first checks local storage for a cached CID result.
   * If missing, it checks for a registered local operator (e.g. primitive generator).
   * If missing locally, it dispatches a Zenoh query over the mesh network to find remote providers.
4. **VFS Metadata Purity**: Files stored on disk use `.data` (binary contents) and `.meta` (JSON descriptor). Metadata is strictly limited to:
   * `state` (AVAILABLE | PENDING)
   * `encoding` (link | json | string | bytes | null)
   * `selector` (original recipe definition)
   * `filename` (user-facing alias string)
