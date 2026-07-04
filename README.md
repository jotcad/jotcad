# JotCAD

JotCAD is a distributed, collaborative geometry processing engine that bridges
C++, Node.js, and Browser environments through a unified **Decentralized
Mesh-VFS**.

## Architecture

The project is built on a **Demand-Driven Mesh Architecture** where different
services (Peers) collaborate through recursive bread-crumb routing and local
content-addressable storage.

### Key Components:

- **Mesh-VFS (`fs/`):** A decentralized, peer-to-peer virtual file system that
  uses deterministic local identity and synchronous request-response to
  coordinate geometry tasks across the network.
- **Geometry Ops (`geo/`):** High-performance geometry kernels (Hexagon, Box,
  Triangle, Offset, Outline) implemented in **C++** and exposed as self-hosting
  Mesh Peers.
- **Services:** Specialized Node.js peers for PDF generation, data export, and
  orchestrating complex geometry pipelines.
- **UX (`ux/`):** A web-based interface that functions as an originator node in
  the mesh for real-time visualization and interactive parameter control.

## Technology Stack

- **Languages:** C++20, Node.js (ESM), Solid.js (UX).
- **Protocol:** Symmetric Bread-crumb Routing over REST.
- **Geometry:** Custom kernels with recursive Mesh resolution.

## Development

### Building C++ Geometry Ops Node

The native geometry kernels (Hexagon, Box, Offset, etc.) are implemented as a 
unified C++ VFS Node. The VFS implementation is split into modular components 
to ensure build stability:
- **`vfs_core`**: Core routing and networking logic.
- **`vfs_primitives`**: Specializations for JSON and binary blobs.
- **`vfs_geo_adapter`**: High-level specializations for `Shape` and `Geometry`.

Compile the source into the `geo/bin/ops` binary:

```bash
cd geo && make && cd ..
```

### Starting the System

Run the orchestration script to start the Peer Nodes (VFS Hub, Ops Node, Export Node) and the UX:

```bash
npm start
```

#### Running with Webcam VFS Integration

If you have a webcam connected (e.g., `/dev/video0`), you can launch the cluster with the webcam VFS node enabled:

```bash
node orchestrator.js webcam
```

This starts the secure VFS Webcam Node, which:
* Registers a lazy, on-demand VFS provider under the path `jot/webcam/capture` (utilizes mutex locking to serialize V4L2 device access and share capture promises).
* Serves a secure HTTPS interface at `https://localhost:8080` with a live reload preview.
* Serves an MJPEG live stream route at `/stream` and single-frame captures at `/image`.
* Supports automated background timelapse capture every 10 seconds, organizing frames into hourly subfolders (`timelapse/YYYY-MM-DD/HH/`) and automatically compiling finished hours into independent `hour_HH.mp4` segments in the background, deleting source JPEGs to optimize disk space.
* Features a decoupled video compiler: `/timelapse/rebuild` triggers compilation (which backfills any missing segments and instantly concatenates hourly MP4s in milliseconds via packet copying), while `/timelapse.mp4` serves the finalized static file using H.264 high keyframe density (`-g 1`) and HTTP range-requests for zero-lag seeking.
* Exposes a frame count API at `/timelapse/count` which efficiently sums segment frame counts from JSON metadata files.
* Integrates with the Zenoh mesh, exposing standard VFS routing endpoints under `https://localhost:8080/vfs`.

## Documentation

- [JotCAD Language Specification](docs/JOT_LANGUAGE_SPECIFICATION.md) - DSL
  syntax, casing rules, and "Tee" pattern.
- [VFS Specification](docs/VFS_SPECIFICATION.md) - Decentralized Mesh protocol
  and identity.
