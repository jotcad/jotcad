# JotCAD

JotCAD is a distributed, collaborative geometry processing engine that bridges C++, Node.js, and Browser environments through a unified Virtual File System (VFS).

## Architecture

The project is built on a **Symmetric Blackboard Architecture** where different services (Peers) collaborate via a central **VFS Hub**.

### Key Components:
- **VFS Hub (`fs/`):** A content-addressable storage and logical routing hub that enables cross-environment coordination and virtual mailboxing for firewalled peers.
- **Geometry Ops (`geo/`):** High-performance geometry kernels (Hexagon, Box, Triangle, Offset, Outline) implemented in **C++** and exposed via the VFS protocol.
- **Services:** Specialized Node.js services for PDF generation, data export, and orchestrating complex geometry pipelines.
- **UX (`ux/`):** A web-based "Blackboard" interface for real-time visualization and interactive parameter control of geometry artifacts.

## Technology Stack
- **Languages:** C++20, Node.js (ESM), TypeScript (UX).
- **Protocol:** Full-Symmetric VFS over REST/SSE.
- **Geometry:** Custom kernels with recursive VFS resolution.

## Getting Started

1.  **Install dependencies:** `npm install`
2.  **Build C++ Ops:** `cd geo && ./build.sh`
3.  **Start the system:** `npm start`
4.  **Run pipeline tests:** `node geo/test/pipeline.test.js`

For more detailed information on the protocol, see [VFS Specification](docs/VFS_SPECIFICATION.md).
