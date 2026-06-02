# Mesh Network Implementation

This directory contains the core logic for the JotCAD decentralized mesh network.

## Components

- **`connection.js`**: Abstract base class for all peer connections.
- **`forward_connection.js`**: Standard HTTP-based outgoing connection.
- **`reverse_connection.js`**: Long-polling based "Reverse" connection for hidden/browser nodes.
- **`mesh_link.js`**: The main Mesh registry, bread-crumb router, and interest manager.

## Protocol Implementation

Refer to `docs/SOVEREIGN_PACKET_PROTOCOL.md` and `docs/WEBSOCKET_VFS_SPECIFICATION.md` for details on the wire protocol.
