# C++ VFS Node Implementation

This directory contains the C++ implementation of the JotCAD Content-Addressable / Computation-Addressable Virtual File System (VFS) and its networking components.

## Subdirectories

- **[vfs](file:///home/brian/github/jotcad/fs/cpp/vfs)**: Core implementation files for HTTP server, connection, and router.
- **[test](file:///home/brian/github/jotcad/fs/cpp/test)**: Tests validating the stability and functionality of the C++ VFS components.
- **[vendor](file:///home/brian/github/jotcad/fs/cpp/vendor)**: Third-party libraries (e.g. `httplib.h`, `json.hpp`).

## File Overview

- **[vfs_core.cpp](file:///home/brian/github/jotcad/fs/cpp/vfs_core.cpp)**: Unified translation unit including connection, router, and server implementations.
- **[vfs_node.h](file:///home/brian/github/jotcad/fs/cpp/vfs_node.h)**: Unified header containing struct definitions, shared helper functions, and declarations for the VFS node.
- **[cid.cpp](file:///home/brian/github/jotcad/fs/cpp/cid.cpp)**: Functions for generating Content Identifiers (CIDs) from math bytes or serialized Selectors.
- **[selector.h](file:///home/brian/github/jotcad/fs/cpp/selector.h)**: Universal addressing structs and normalization routines.
- **[vfs_primitives.cpp](file:///home/brian/github/jotcad/fs/cpp/vfs_primitives.cpp)**: Implementation of primitive geometries within the VFS.
