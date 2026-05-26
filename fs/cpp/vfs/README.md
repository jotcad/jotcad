# C++ VFS (Virtual File System) Implementation Directory

This directory contains clean, atomic files that implement the decentralized VFS Node in JOTCAD. 

## Architectural Files

- **[server.cpp](file:///home/brian/github/jotcad/fs/cpp/vfs/server.cpp)**: Handles HTTP REST endpoints, secure contexts (SSL Server), and request route parsing.
- **[router.cpp](file:///home/brian/github/jotcad/fs/cpp/vfs/router.cpp)**: Manages local data materialization, link tracking, registry matching, interest propagation, and selective materialization of selector computational chains.
- **[connection.cpp](file:///home/brian/github/jotcad/fs/cpp/vfs/connection.cpp)**: Implements outgoing `ForwardConnection` REST interfaces and incoming `ReverseConnection` long-polling/tunneling queue managers.
