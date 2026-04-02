# Blackboard VFS Architecture

`@jotcad/fs` is a distributed, idempotent, and stream-based virtual filesystem. It acts as a blackboard (or tuple space) that completely decouples the *desire* for data from the *computation* of that data.

## Core Concepts

### 1. The Blackboard Model
Nodes do not communicate directly. They read from and write to a shared virtual coordinate space.
- **Consumers** express demand for a coordinate.
- **Producers** fulfill that demand by writing data to the coordinate.

### 2. Dual Planes (Request & Result)
- **The Request Plane (`/req/`)**: Manifests "Provisioning Requests" (Demand).
- **The Result Plane (`/res/`)**: Stores immutable artifacts (Results).

### 3. Constraints & Selectors
Data is addressed by a base `path` and a structured `parameters` object.
- **Fully Constrained (Points):** Exact path and parameters. Deterministic and idempotent.
- **Partially Constrained (Regions):** Open parameters or glob paths. Used for observation (`watch`).

### 4. Namespaces and Isolation
- **Strict Isolation:** Different sessions are independent and do not cross-talk.
- **Coupled Instances (Distributed):** A single session can span multiple environments.
- **RESTful Transport:** Node-to-Browser communication leverages standard HTTP.
    - **Results (`/res/:cid`)**: Uses `GET` and `PUT` with binary streams. No JSON serialization overhead.
    - **Events (`/watch`)**: Uses **Server-Sent Events (SSE)** to push state changes from the server to clients.
- **Shared Storage:** Instances in the same browser origin share an **IndexedDB** Result Plane.

### 6. Synchronous VFS (SyncVFS)
For high-performance computational agents (e.g., C++ or WASM) that require a synchronous, POSIX-like filesystem interface, the system provides a **SyncVFS** abstraction.
- **Blocking I/O:** `SyncVFS.read()` and `SyncVFS.write()` block the calling thread until the operation is complete. This allows C++ code to use standard `std::ifstream` without becoming asynchronous.
- **Environment Implementations:**
    - **Browser/WASM:** Implemented using **Atomics + SharedArrayBuffer**. The WASM module runs in a Web Worker and "faults" to the VFS thread, which uses `Atomics.wait()` to pause the worker until data arrives.
    - **Node.js:** Implemented using **Worker Threads** or **Synchronous REST calls**, allowing for native-speed blocking I/O.
- **Uniform API:** The C++ code interacts with a single "File Driver" that serializes VFS selectors into URI-style paths (e.g., `vfs:/box?size=100`).

## Core Primitives

### `tickle(selector)`
Manifests demand for a coordinate. Returns the current state immediately.

### `read(selector)`
Blocks until the coordinate state reaches `AVAILABLE`. Returns a stream.

### `write(selector, stream)`
Finalizes a computation. Relays metadata and data (small payloads) to peers.

### `lease(selector, duration)`
Secures an exclusive, time-limited window for a processor to compute a result.

### `watch(selector)`
Returns an Async Iterator of state changes within a region of the coordinate space.

---

## Progress Report (April 2026)

- [x] **Core VFS:** In-memory blackboard with state propagation.
- [x] **Node Abstraction:** Socket DSL (`In`, `Out`, `Watch`) for agent-style nodes.
- [x] **Self-Describing Storage:** Enveloped artifacts (`.meta` + `.data`) for introspection.
- [x] **Memory Offloading:** Disk-based storage for Node.js.
- [x] **Shared Browser Storage:** IndexedDB-based storage shared across Browser, Worker, and Service Worker.
- [x] **Cross-Environment Coordination:** Data relaying for crossing JSON-only boundaries (postMessage, WebSocket).
- [x] **Multi-Environment Mesh:** Verified coordination across Node, Browser, Web Worker, and Service Worker.
- [x] **Graceful Failure:** Suicide-pattern for agents when the VFS closes.

---

## Future Research

> **Storage Decomposition:** Implement prefix-based sharding (e.g., `res/54/e660d4...`) for high-volume results.
> **True P2P Streaming:** Explore WebRTC or binary WebSocket streams for large artifacts that shouldn't be buffered.
> **Logical Clocks:** Use sequence numbers or vector clocks to ensure consistent state resolution in complex meshes.
