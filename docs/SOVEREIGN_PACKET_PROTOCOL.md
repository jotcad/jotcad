# Sovereign Packet Protocol Specification

## Motivation

The JotCAD mesh network requires a unified architecture where physical hardware sensors (e.g., ESP32-CAM) can participate as first-class peers alongside C++ native nodes and JS web clients. Previously, the VFS protocol relied heavily on wrapping payloads inside structured JSON objects (e.g., `{"selector": {...}, "payload": <data>}`). 

This JSON-wrapping approach severely degraded performance and memory efficiency for constrained devices sending raw binary data (such as a 2 FPS JPEG stream from an ESP32-CAM), forcing them to Base64 encode binary data or parse heavy JSON envelopes.

The **Sovereign Packet Protocol** separates routing metadata from the actual data payload. Metadata is hoisted into HTTP headers, and the HTTP body is left strictly for the uninterpreted payload (raw binary bytes).

## Core Directives

1. **Header-Driven Routing**: Transport metadata MUST be transmitted using plain strings or JSON strings inside dedicated HTTP headers.
2. **Pure Payloads**: The HTTP body MUST be exactly the payload. JSON wrapping (`{ "payload": ... }`) is strictly prohibited during network transport.
3. **No Structural Guessing**: Peer handlers MUST route using the Selector provided in the headers rather than inspecting or analyzing the payload structure.
4. **Binary First**: If `X-VFS-Encoding: bytes` is present, the body is treated as a raw binary buffer/stream. No Base64 encoding is permitted over the network.

## HTTP Headers

Every Sovereign Packet traversing the mesh MUST utilize the following headers where applicable:

| Header | Description | Example |
| :--- | :--- | :--- |
| `X-VFS-Op` | The operation being performed (`PUB`, `READ`, `SUB`). Required for Reverse Tunnels. | `PUB` |
| `X-VFS-Selector` | The stringified JSON representation of the `Selector` addressing the request. | `{"path":"sensor/cam","parameters":{}}` |
| `X-VFS-Stack` | Comma-separated list of peer IDs the packet has traversed (loop prevention). | `nodeA,nodeB` |
| `X-VFS-Encoding` | Hint for how to process the body (`json` or `bytes`). | `bytes` |
| `X-VFS-Expires` | (Optional) Epoch timestamp in milliseconds when the request/subscription expires. | `1779805684471` |
| `X-VFS-Peer-Id` | The ID of the node establishing a reverse tunnel. | `esp32_cam_01` |

## Endpoints

### 1. `/register` (Handshake)
Initiates a mesh connection.
* **Method**: `POST`
* **Body**: JSON object containing `id` and `url`. `{"id":"node_b","url":"http://localhost:8182"}`
* **Response**: `200 OK` with JSON `{"id": "node_a", "reachability": "DIRECT"}`.

### 2. `/listen` (Reverse Tunnel)
Used by peers behind NATs/Firewalls (or clients lacking open ports) to establish a long-lived polling connection.
* **Method**: `POST`
* **Headers**: `X-VFS-Peer-Id: <id>`
* **Behavior**: The server holds the connection open until it needs to send a packet to the peer. If no packet is ready, it waits. If a new poll arrives from the same peer, the old one is superseded and closed with `204 No Content`.
* **Response**: A Sovereign Packet (with headers and body) destined for the polling peer. The `X-VFS-Op` header informs the client what action to take (e.g., `PUB`, `READ`).

### 3. `/notify` (Push Data)
Pushes data into the mesh. (This is the HTTP endpoint for `PUB` operations).
* **Method**: `POST`
* **Headers**: `X-VFS-Op: PUB`, `X-VFS-Selector`, `X-VFS-Encoding`, `X-VFS-Stack`
* **Body**: Raw bytes.

### 4. `/read` (Pull Data)
Requests data from the mesh.
* **Method**: `POST`
* **Headers**: `X-VFS-Op: READ`, `X-VFS-Selector`, `X-VFS-Stack`, `X-VFS-Expires`
* **Body**: (Empty or JSON containing `cid`/`resolutionStack` if CID-based read).
* **Response**: A Sovereign Packet containing the requested data.

### 5. `/subscribe` (Persistent Pull)
Subscribes to a stream of updates for a given selector.
* **Method**: `POST`
* **Headers**: `X-VFS-Op: SUB`, `X-VFS-Selector`, `X-VFS-Stack`, `X-VFS-Expires`

## ESP32 / Embedded Constraints
- **Recursive Mutex**: ESP32 operations must utilize `std::recursive_mutex` due to re-entrant mesh link logic.
- **Task Isolation**: The `/listen` mesh server loop must run in a dedicated FreeRTOS task on Core 0 to prevent starving geometry operations on Core 1.
