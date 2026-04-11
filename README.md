# JotCAD

JotCAD is a distributed, collaborative geometry processing engine that bridges C++, Node.js, and Browser environments through a unified **Decentralized Mesh-VFS**.

## Architecture

The project is built on a **Demand-Driven Mesh Architecture** where different services (Peers) collaborate through recursive bread-crumb routing and local content-addressable storage.

### Key Components:
- **Mesh-VFS (`fs/`):** A decentralized, peer-to-peer virtual file system that uses deterministic local identity and synchronous request-response to coordinate geometry tasks across the network.
- **Geometry Ops (`geo/`):** High-performance geometry kernels (Hexagon, Box, Triangle, Offset, Outline) implemented in **C++** and exposed as self-hosting Mesh Peers.
- **Services:** Specialized Node.js peers for PDF generation, data export, and orchestrating complex geometry pipelines.
- **UX (`ux/`):** A web-based interface that functions as an originator node in the mesh for real-time visualization and interactive parameter control.

## Technology Stack
- **Languages:** C++20, Node.js (ESM), Solid.js (UX).
- **Protocol:** Symmetric Bread-crumb Routing over REST.
- **Geometry:** Custom kernels with recursive Mesh resolution.

## Getting Started

1.  **Install dependencies:** `npm install`
2.  **Build C++ Ops:** `cd geo && ./build.sh`
3.  **Start the system:** `npm start`
4.  **Run pipeline tests:** `node geo/test/pipeline.test.js`

For more detailed information on the protocol, see [VFS Specification](docs/VFS_SPECIFICATION.md).
