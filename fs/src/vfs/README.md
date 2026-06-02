# Virtual File System (VFS) Implementation

This directory contains the core logic for the JotCAD VFS.

## Components

- **`vfs.js`**: The main VFS class and public API.
- **`schema.js`**: Selector validation and schema enforcement.
- **`resolver.js`**: Recursive fulfillment and mesh routing logic.

## Design

The VFS is a content-addressable storage system with a computational overlay (Selectors).
It handles recursive fulfillment by routing requests to local providers or mesh neighbors.
