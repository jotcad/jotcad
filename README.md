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

The native geometry kernels (Hexagon, Box, Offset, etc.) are implemented as a unified C++ VFS Node. Compile the source into the `geo/bin/ops` binary:

```bash
cd geo && ./build.sh && cd ..
```

### Starting the System

Run the orchestration script to start the Peer Nodes (VFS Hub, Ops Node, Export Node) and the UX:

```bash
npm start
```

## Documentation

- [JotCAD Language Specification](docs/JOT_LANGUAGE_SPECIFICATION.md) - DSL
  syntax, casing rules, and "Tee" pattern.
- [VFS Specification](docs/VFS_SPECIFICATION.md) - Decentralized Mesh protocol
  and identity.
