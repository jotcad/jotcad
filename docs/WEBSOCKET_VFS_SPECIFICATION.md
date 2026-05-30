# VFS WebSocket Transport Layer Specification

This document defines the architecture, message formats, handshake negotiation, and microcontroller (PIO) integration for an opportunistic WebSocket transport layer within the JotCAD Virtual File System (VFS) mesh.

This transport layer is designed to run in parallel with the existing HTTP/REST and Long-Polling mechanisms, upgrading directly to WebSocket whenever negotiable, and falling back gracefully when needed.

---

## 1. Design Goals

- **Bi-directional Low-Latency Multiplexing**: Eliminate HTTP request/response connection overhead for high-frequency virtual file lookups and mesh event propagates.
- **Pure Push-Based Reverse Connections**: Eliminate battery/CPU-draining HTTP long-polling loops for nodes behind NAT/firewalls or browser UX contexts by utilizing the persistent bi-directional channel.
- **Opportunistic Upgrade**: Seamlessly negotiate and transition from HTTP to WebSocket connections during initial registration.
- **Hardware Agnostic (PIO Integration)**: Standardize on a lightweight, memory-efficient framing format suitable for constrained microcontrollers (ESP32 and ESP8266) running freeRTOS or bare-metal loops.

---

## 2. Negotiation & Connection Upgrade

WebSocket support is negotiable at registration time. The existing `POST /register` handshake payload is extended with capability advertising.

### A. Handshake Capabilities

When registering a peer, nodes advertise supported protocols in a `transports` list and expose a `wsUrl` target endpoint.

#### 1. Registration Request
```json
{
  "id": "node_a",
  "url": "http://192.168.1.100:9591",
  "transports": ["http", "ws"]
}
```

#### 2. Registration Response
```json
{
  "id": "node_b",
  "reachability": "DIRECT",
  "transports": ["http", "ws"],
  "wsUrl": "ws://192.168.1.101:9592/vfs-ws"
}
```

### B. Upgrade Workflow

1. If both peers support `"ws"`, the registering client initiates a WebSocket connection to the server's `wsUrl`.
2. Once the TCP socket is upgraded to WebSocket, the client transmits an **`IDENTIFY`** frame.
3. The server validates the identity and responds with an **`ACK`** frame.
4. The system transitions routing for that specific peer from the HTTP adapter to the newly created WebSocket adapter.
5. If the WebSocket connection fails at any stage (timeout, proxy block, SSL failure), both nodes **gracefully fall back** to the HTTP REST and Long-Polling adapters.

```
       [HTTP/REST Register Handshake]
                      │
            Both Support WebSocket?
             ├── No  ──► [Use Standard HTTP/REST + Long-Poll]
             └── Yes ──► [Initiate ws:// Connection to /vfs-ws]
                              │
                      Connection OK?
                       ├── No  ──► [Fallback to HTTP/REST]
                       └── Yes ──► [Send IDENTIFY -> Receive ACK]
                                        │
                               [WebSocket Transport Active]
```

---

## 3. Multiplexed Messaging Protocol (Framing)

Since WebSocket channels are asynchronous, request-response exchanges must be correlated using dynamic transaction identifiers (`txId`).

### A. Core Message Envelope (JSON)

Every frame sent over the WebSocket transport adheres to the layout below. To maximize efficiency and avoid Base64 overhead for CAD artifacts, binary data is transmitted using **Sequential Framing**.

```json
{
  "txId": "msg_f7c1a892b",    // Unique transaction correlation ID (required for READ/READ_RESPONSE)
  "type": "IDENTIFY" | "ACK" | "READ" | "READ_RESPONSE" | "SUBSCRIBE" | "NOTIFY" | "SPY" | "SPY_RESPONSE",
  "selector": {                // Safe-JCB Selector representation
    "path": "sys/topo",
    "parameters": {}
  },
  "cid": "...",                // Content Identifier (populated for direct-hash reads)
  "payload": {},               // Payload content (JSON, schema metadata, or serialized maps)
  "hasBinary": false,          // IF TRUE: The very next WebSocket frame MUST be a BINARY frame containing the data.
  "stack": [],                 // Cycle prevention array
  "expiresAt": 0,              // TTL or subscription expiry timestamp (epoch ms)
  "status": 200                // Execution status code (for READ_RESPONSE)
}
```

### B. Sequential Binary Framing

To avoid the 33% size inflation of Base64, JotCAD uses an atomic sequential framing protocol for binary payloads (e.g., STLs, PNGs, Binary Mesh data):

1. **The Header (TEXT Frame)**: A standard JSON envelope is sent as a text frame. It MUST contain `"hasBinary": true` and MUST NOT contain a `payload.data` field.
2. **The Payload (BINARY Frame)**: The raw bytes are sent as the immediately following WebSocket binary frame.

**Sender Constraints**:
- Senders MUST ensure that the Text Header and Binary Payload are transmitted atomically. In multi-threaded environments (like Native C++), the socket MUST be locked (Mutex) during this sequence to prevent interleaving frames from other threads.
- In single-threaded asynchronous environments (like JavaScript), the two `send()` calls must be executed synchronously back-to-back.

**Receiver Constraints**:
- If a receiver parses a JSON text frame with `"hasBinary": true`, it MUST enter a "Waiting for Binary" state.
- The receiver MUST treat the *very next* frame received on that socket as the binary payload for the previous header, regardless of any other pending transactions.
- Once the binary frame is received, the message is considered complete and is dispatched for processing.

### C. Flow Primitives

#### 1. Asynchronous Read Request (`READ`)
Initiated by either peer. Requires `txId` for tracking.
```json
{
  "txId": "read_00918ac",
  "type": "READ",
  "selector": { "path": "sensor/temperature", "parameters": {} },
  "stack": ["gateway_node"],
  "expiresAt": 177989912000
}
```

#### 2. Read Response (`READ_RESPONSE`)
Returns requested resource bytes and metadata correlated by `txId`.
```json
{
  "txId": "read_00918ac",
  "type": "READ_RESPONSE",
  "status": 200,
  "payload": {
    "data": "MjEuNQA=",      // Base64-encoded file payload (e.g., 21.5)
    "metadata": {
      "encoding": "string",
      "state": "AVAILABLE"
    }
  }
}
```

#### 3. Interest Propagation (`SUBSCRIBE`)
Paints a subscription trail in the mesh. No reply expected.
```json
{
  "type": "SUBSCRIBE",
  "selector": { "path": "sys/topo", "parameters": {} },
  "expiresAt": 177989912000,
  "stack": ["subscriber_node"]
}
```

#### 4. Event Notification (`NOTIFY`)
Broadcasts dynamic updates down a subscription path.
```json
{
  "type": "NOTIFY",
  "selector": { "path": "sys/topo" },
  "payload": {
    "peer": "gateway_node",
    "neighbors": [
      { "id": "esp32_sensor", "reachability": "DIRECT" }
    ]
  },
  "stack": []
}
```

---

## 4. PIO Integration (ESP32 & ESP8266 Microcontrollers)

Constrained microcontrollers face significant limitations regarding heap space, parsing buffers, and network timeouts. Persistence via WebSockets resolves CPU and memory overhead caused by repeated socket churn.

### A. Hardware-Specific Architecture

```
                  ┌───────────────────────────────┐
                  │      JotCAD Gateway Node      │
                  └───────────────▲───────────────┘
                                  │
                       WebSocket Persistent
                        (Frames < 1.5 KB)
                                  │
                  ┌───────────────▼───────────────┐
                  │    ESP32 / ESP8266 Client     │
                  │   ┌───────────────────────┐   │
                  │   │   VFS Router (Core)   │   │
                  │   └───────────▲───────────┘   │
                  │               │               │
                  │   ┌───────────▼───────────┐   │
                  │   │  JSON Selector Map    │   │
                  │   └───────────────────────┘   │
                  └───────────────────────────────┘
```

### B. Lightweight Microcontroller Constraints

1. **Persistent Socket Reuse**: Microcontrollers establish an outbound WebSocket connection to the VFS Gateway using a optimized WebSocket client wrapper (e.g. ESP-IDF native WebSockets component or `arduinoWebSockets` library).
2. **Buffer Limits**: Large CAD payloads (STLs, high-resolution PDFs) can easily overflow a microcontroller's SRAM. PIO nodes **MUST** restrict incoming WebSocket frames to $1.5\text{ KB}$ (matching standard MTUs). Payload fragments larger than $1.5\text{ KB}$ must be chunked or rejected.
3. **Zero-Copy Parser (`ArduinoJson`)**: VFS JSON envelopes are parsed using fixed stack-allocated `StaticJsonDocument` or `DynamicJsonDocument` allocations (typically sized at $1024$ bytes) to prevent dynamic heap fragmentation.
4. **Active Connection Keep-Alives**: Since microcontrollers may sit behind strict routers, the PIO node must transmit a 2-byte ping frame every 15 seconds to ensure network state consistency.

### C. PIO Hardware Command Execution Flow

Through the bi-directional reverse transport, the VFS server can push commands down the microcontroller's socket to execute real-time pin states or retrieve sensor telemetry.

#### Scenario: Server Requests Real-Time Sensor Telemetry
```
  [Server / Gateway]                            [PIO Node (ESP32)]
          │                                              │
          │ ─── (WS: READ "sensor/counter") ───────────► │
          │                                              │ ─── Read GPIO pins / 
          │                                              │     hardware timers
          │                                              │
          │ ◄── (WS: READ_RESPONSE "200: [Value]") ──────│
```

1. The gateway executes `vfs.read({ path: "sensor/counter" })`.
2. The router resolves this selector to the PIO node and fires a `READ` frame over the open WebSocket.
3. The PIO node receives the WebSocket frame, parses the JSON structure on the stack, and routes it directly to its local sensor loop.
4. The PIO node reads its physical GPIO pin/timer registers, formats the result as a raw value, base64 encodes it, and pushes the `READ_RESPONSE` frame back up the WebSocket channel.

---

## 5. Native C++ Implementation Guidelines (VFS Core)

Native nodes (C++) utilize the WebSocket transport to provide the backbone of the mesh. High-performance native implementations prioritize memory safety and non-blocking I/O.

### A. Server Lifecycle & Coexistence

1. **Dual-Stack Server**: Native nodes run a standard HTTP server (e.g., `httplib`) for baseline Sovereign Packet requests and a parallel WebSocket server (e.g., `IXWebSocket`) on the same or a coordinated port.
2. **Upgrade Hijacking**: If possible, the HTTP server should detect the `Connection: Upgrade` header and hand off the socket to the WebSocket handler.
3. **Peer Mapping**: The native `VFSNode` maintains a thread-safe `std::unordered_map<std::string, std::shared_ptr<WebSocketConnection>>` to route traffic to the appropriate persistent tunnel.

### B. Threading & Concurrency

1. **Non-blocking Event Loop**: Incoming WebSocket frames are processed in a dedicated background thread pool.
2. **Atomic Transaction Correlation**: A thread-safe `std::unordered_map<std::string, std::promise<VFSResponse>>` is used to park pending `READ` requests while waiting for their asynchronous `READ_RESPONSE` from the remote peer.
3. **CID Deduplication**: WebSocket frames containing JSON payloads MUST be hashed using the standard `JCB` (JotCAD Canonical Binary) algorithm before being committed to the native cache to ensure CID consistency across C++ and JS nodes.

---

## 6. Security & Context-Safe Transports

- **HTTPS / WSS Upgrades**: When HTTPS mode is active in the mesh (`DISABLE_SSL=0`), the registration negotiator upgrades the URL scheme to secure WebSockets: `wss://`.
- **Certificates on Embedded Devices**: Microcontrollers must include the gateway's SSL certificate fingerprint or CA Root in their flash storage to secure the persistent WSS connection, keeping communication authentic and encrypted across public boundaries.
- **Native SSL Contexts**: C++ implementations utilizing OpenSSL MUST share the same SSL context/certificates between the HTTP and WebSocket listeners to ensure consistent identity verification.
