# Blackboard VFS Architecture

`@jotcad/fs` is a distributed, idempotent, and stream-based virtual filesystem.
It acts as a blackboard (or tuple space) that completely decouples the _desire_
for data from the _computation_ of that data.

## Core Concepts

### 1. The Blackboard Model

Nodes do not communicate directly. They read from and write to a shared virtual
coordinate space.

- **Consumers** express demand for a coordinate.
- **Producers** fulfill that demand by writing data to the coordinate.

### 2. Content Addressing (CID)

Every artifact on the blackboard is addressed by a **Content-ID (CID)**. The CID
is a SHA-256 hash of the concatenated **Path** and **Normalized Parameters**.

**Formula:** `SHA256(path + JSON.stringify(sort_keys(parameters)))`

**Implementation Rules:**

1.  **Parameters:** Must be a flat JSON object. Keys must be sorted
    alphabetically before stringification.
2.  **Stringification:** No spaces should be present in the JSON string (e.g.,
    `{"a":1,"b":2}`).
3.  **Concatenation:** The path is prepended directly to the JSON string.

### 3. Dual Planes (Request & Result)

- **The Request Plane (`/req/`)**: Manifests "Provisioning Requests" (Demand).
- **The Result Plane (`/res/`)**: Stores immutable artifacts (Results).

### 3. Constraints & Selectors

Data is addressed by a base `path` and a structured `parameters` object.

- **Fully Constrained (Points):** Exact path and parameters. Deterministic and
  idempotent.
- **Partially Constrained (Regions):** Open parameters or glob paths. Used for
  observation (`watch`).

## The Lifecycle States

Any given (path, parameters) point exists in one of five states:

1. **`MISSING`**: No request exists for this point.
2. **`PENDING`**: A node has requested this point, but no processor holds an
   active lease.
3. **`PROVISIONING`**: A processor holds a time-limited lease and is actively
   computing/streaming.
4. **`LINKED`**: The point is a formal, unambiguous alias for another coordinate.
   Link resolution is metadata-driven; the VFS follows the `target` selector 
   defined in the coordinate's envelope. Implicit "guessing" of links from data
   content is forbidden.
5. **`AVAILABLE`**: The computation is complete, and the artifact is ready in
   the Result Plane.

## Architecture Layers

The system is strictly divided into layers to ensure that computational logic
remains agnostic of network and environment details.

### 1. The Agent Layer (Logic)

Computational agents (Processors) interact only with the **Blackboard API**
(e.g., via the `Node` socket abstraction or `SyncVFS`). They are completely
unaware of whether data is local, remote, or being relayed through a mailbox.

### 2. The VFS Layer (Orchestration)

The core `VFS` class manages state transitions, leases, and local event
propagation. It handles the mapping of human-readable coordinates to internal
Content-IDs (CIDs) and maintains the idempotency of the Result Plane.

### 3. The Transport Layer (Communication)

Bridges and Servers handle the distribution of state and data across process and
network boundaries.

- **RESTful Symmetry:** Communication between coupled instances (e.g., Node and
  Browser) leverages a symmetric REST + SSE protocol.
- **Virtual Mailboxes:** Peers behind firewalls or in browsers maintain a
  long-lived **Server-Sent Events (SSE)** connection to a publicly reachable
  **Hub**. The Hub relays inbound requests to these private peers via their
  virtual mailbox.
- **Transparent Relaying:** When a Peer receives a command (like `READ`) via its
  mailbox, the transport bridge automatically fulfills it from local storage,
  sending the data back to the Hub without the Agent ever being involved.

### 4. The Storage Layer (Persistence)

Pluggable adapters offload data from RAM to persistent or shared storage.

- **MemoryStorage:** Standard in-memory Map.
- **DiskStorage:** Uses `.meta` (JSON) and `.data` (Binary) files for
  introspectable persistence in Node.js.
- **IndexedDBStorage:** Shared storage across Browser origins (Main Thread,
  Workers, SW).

### 5. Sandbox Marshalling

To keep core computational logic (e.g., C++ or WASM) decoupled from the VFS,
agents use **Temporary Sandboxes**.

- **Private Workspace:** Each operation runs in its own private, ephemeral
  filesystem (In-memory via `memfs` or on-disk via temporary directories).
- **Marshalling (Global $\rightarrow$ Local):** The JS agent fetches required
  artifacts from the VFS and writes them to the sandbox with local,
  operation-specific filenames (e.g., `in/mesh.stl`).
- **Execution:** The operation runs against the sandbox, reading from and
  writing to local files.
- **Un-marshalling (Local $\rightarrow$ Global):** The JS agent reads the
  finalized results from the sandbox and calls `vfs.write()` to distribute them
  back to the global Result Plane.
- **Cleanup:** The sandbox is purged once the distribution is complete.

### 6. ZFS (Filesystem Snapshots)

To export a portion of the Blackboard for external consumption or archiving, the
system uses the **ZFS** format.

- **Archive Pattern:** ZFS uses the JOT entry framing
  (`\n=<LENGTH> <FILENAME>\n<CONTENT>`).
- **Coordinate Envelopes:** For every coordinate in the exported subgraph, ZFS
  stores:
  - `vfs/[cid].meta`: JSON containing the `path`, `parameters`, `state`, and
    optional `target` (for links).
  - `vfs/[cid].data`: The raw binary bytes (for `AVAILABLE` artifacts).
- **Self-Contained Subgraph:** `vfs.snapshot(selector)` performs a recursive
  crawl, following both explicit `LINKED` states and implicit `vfs:/` references
  found within the data, ensuring the entire dependency tree is captured.

## Core Primitives (Blackboard API)

These are the only methods used by Agents.

### `tickle(selector)`

Manifests demand for a coordinate. Returns the current state immediately.

### `read(selector)`

Blocks until the coordinate state reaches `AVAILABLE` or `LINKED`. If the state
is `LINKED`, it recursively resolves the formal `target` selector from the 
metadata. This resolution is unambiguous and strictly metadata-driven.

### `link(source, target)`

Marks the source coordinate as an alias for the target coordinate. Used for
parameter normalization.

### `write(selector, stream)`

Finalizes a computation and places the result on the global Result Plane.

### `lease(selector, duration)`

Secures an exclusive, time-limited window for a processor to compute a result.

### `watch(selector)`

Returns an Async Iterator of state changes within a region of the coordinate
space.

---

## Progress Report (April 2026)

- [x] **Core VFS:** In-memory blackboard with state propagation.
- [x] **Node Abstraction:** Socket DSL (`In`, _Out_, `Watch`) for agent-style
      nodes.
- [x] **Self-Describing Storage:** Enveloped artifacts (`.meta` + `.data`) for
      introspection.
- [x] **Memory Offloading:** Disk-based storage for Node.js.
- [x] **Shared Browser Storage:** IndexedDB-based storage shared across Browser,
      Worker, and Service Worker.
- [x] **Symmetric REST Transport:** Cross-environment coordination via REST,
      SSE, and Virtual Mailboxes.
- [x] **SyncVFS Bridge:** Atomics-based blocking I/O for synchronous WASM/C++
      agents.
- [x] **Multi-Environment Mesh:** Verified coordination across Node, Browser,
      Web Worker, and Service Worker.
- [x] **Graceful Failure:** Suicide-pattern for agents when the VFS closes.
- [x] **Parameter Normalization:** Support for `LINKED` state and recursive
      alias resolution.

---

## Future Research

> **Storage Decomposition:** Implement prefix-based sharding (e.g.,
> `res/54/e660d4...`) for high-volume results. **Logical Clocks:** Use sequence
> numbers or vector clocks to ensure consistent state resolution in complex
> meshes.
