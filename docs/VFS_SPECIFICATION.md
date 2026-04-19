# JotCAD Virtual File System (VFS) Specification: Decentralized Mesh

The JotCAD VFS is a distributed, content-addressable mesh system designed for
collaborative geometry processing across C++, Node.js, and Browser environments.
It uses a **Decentralized Peer-to-Peer** architecture where participants
coordinate through recursive bread-crumb routing and local content-addressable
storage.

## 1. Identity & Addressing

The VFS uses a **Symmetric Identity** model where the request itself serves as
the global address.

### 1.1 The Selector (Wire Identity)

The global identity of any artifact is its **Selector**: a combination of a
`path` (e.g., `shape/hexagon`) and a `parameters` object (e.g.,
`{"diameter": 100}`).

- **Normalization:** Selectors are deeply and recursively normalized. Parameters
  are sorted alphabetically by key, and nested objects are themselves
  normalized.
- **Parametric Standardization:** Primitives must normalize equivalent
  parameters to a single canonical key before hashing. For example, `radius: 50`
  and `apothem: 43.3` must both be converted to `diameter: 100`.
- **Deterministic Identity:** Every node calculates the identity of a request
  locally by hashing the normalized selector (SHA-256).

### 1.2 Identity Secrecy

To prevent "Identity Poisoning," CIDs (Content IDs) are NEVER sent across the
network. Nodes only exchange Selectors. Each node is the sole authority over its
local CID mapping.

## 2. Peer-to-Peer Protocol (Routing)

### 2.1 Formal Links (Unambiguous Aliasing)

The VFS supports **Formal Links**, a mechanism for aliasing one Selector to another. This is used for parametric standardization and canonical coordinate mapping.

- **Link Definition:** A Link is a local artifact whose metadata contains a `target` Selector and whose state is marked as `LINKED`.
- **Recursive Resolution:** When `vfs.read()` encounters a `LINKED` artifact, it recursively initiates a new `read` for the `target` Selector.
- **Loop Protection:** Recursion is limited to a maximum depth (default: 10) to prevent infinite aliasing loops.
- **Content Pointer:** The data payload of a Link typically contains a fallback text pointer (e.g., `vfs:/path/to/target`) for environments that do not support metadata resolution.

### 2.2 Recursive Bread-crumb READ (`POST /read`)

The `read` operation is the primary mechanism for demand-driven data retrieval.

- **Unambiguous Signaling (Throwing Model):** A `read` MUST either return the requested data with a `200 OK` or fail with an explicit HTTP error code. 
- **Error Format:** Failures are returned as JSON objects with `type: "sys/error"`.
  - `404 Not Found`: Artifact does not exist in the mesh.
  - `408 Request Expired`: The `expiresAt` timestamp has passed.
  - `400 Bad Request`: Validation failure or routing loop detected.
  - `508 Loop Detected`: Recursive link depth exceeded.

### 2.3 Long-Polling Reverse Connections

To prevent "Hot Loop" flooding, reverse connection providers use long-polling.

- **Blocking Wait:** The server MUST hold a `/listen` request open until a command is available or a timeout occurs (default: 30s).
- **Timeout Response:** If no command is available within the timeout period, the server MUST return `204 No Content`. Requesters SHOULD immediately re-initiate the `/listen` request.
- **Notification Enqueuing:** Notifications for reverse peers are enqueued as `NOTIFY` commands in the same queue as `READ` commands.


### 2.2 Demand-Driven Work

The VFS is **Silent by Default**. No work or network traffic occurs until a
`read` is initiated.

- **Provider Registration:** Nodes register handlers for specific path prefixes.
- **Implicit Trigger:** A `read` request for a registered path automatically
  triggers the local provider if the data is missing from disk.

## 3. Immunity & Lifecycle

### 3.1 Ephemeral Lifecycle (The Clean Slate)

Every VFS node starts with an empty local storage.

- **Initialization Wipe:** On startup (`VFS.init`), the node physically deletes
  all files in its `.vfs_storage` directory.
- **Immunity:** This prevents "Phantom Paths" and stale metadata from previous
  sessions from contaminating the current mesh.

### 3.2 Local Sovereignty

A node is the absolute authority over its local disk. It only stores data that
it has calculated locally or received as a direct response to a request it
initiated or forwarded.

## 4. Environment Robustness

The VFS is **Web Stream Standardized**:

- **Unified Streams:** All data transfer uses the **Web Streams API**
  (`ReadableStream`).
- **Cross-Environment:** This ensures seamless data flow between C++ (via Node
  wrapper), Node.js services, and the Browser UX.

## 5. Reverse Connections (Listen & Respond)

To allow browsers and other nodes behind NAT/Firewalls to act as providers, the
VFS supports **Reverse Connections** via a symmetric Long-Poll mechanism.

### 5.1 Diagnostic Endpoints

All VFS nodes must support standard diagnostic endpoints for health and version checking:

-   **`GET /health`**: Returns `{"status": "OK", "id": "peer-id"}`. Used by monitoring and peering services to verify node availability.
-   **`GET /version`**: Returns `{"version": "1.1.0", "id": "peer-id"}`. Used to ensure protocol compatibility across the mesh.

### 5.2 The Listen Loop (`POST /listen`)

A "Hidden Node" establishes a persistent command channel with a "Visible Node"
(Server) by initiating a long-poll request.

- **Endpoint:** `POST /vfs/listen?peerId={UUID}`
- **Symmetric Exchange:**
  - **Inbound (Browser -> Server):** The `POST` body carries the binary result
    of the _previous_ command. The `x-vfs-reply-to` header contains the
    `requestId`.
  - **Outbound (Server -> Browser):** The `200 OK` response body carries the
    JSON definition of the _next_ command.

### 5.2 Command Execution

When the mesh routes a `read` to a Hidden Node:

1.  **Dispatch:** The Visible Node retrieves the pending `/listen` response for
    that `peerId`.
2.  **Command:** It writes a `READ` command (Path, Parameters, RequestID) to the
    response and closes it.
3.  **Wait:** The Visible Node holds the original mesh-read request open,
    awaiting a new `/listen` POST from the browser that references the
    `requestId`.

### 5.3 Topology Sourcing

Hidden Nodes are identified by their **UUID**. The `MeshLink` maintains a
mapping of UUIDs to their currently active `/listen` slots, allowing the
bread-crumb router to treat them as standard neighbors.

## 6. Discovery Protocol (Spy)

The VFS strictly separates **Execution (Demand)** from **Observation
(Discovery)** to prevent infinite computation loops when encountering ambiguous
requests.

### 6.1 The "Fail Fast" Demand Law

The `vfs.read()` operation represents **Demand**. A Demand selector MUST be
fully constrained (either natively or via `.spec()` canonicalization). If a
provider receives an underconstrained selector during a `read` (e.g., missing
required parameters without defaults), it MUST reject the request synchronously.
The VFS never guesses.

### 6.2 The Spy Protocol (`vfs.spy()`)

To explore the network or find multiple artifacts, nodes use the `spy` protocol.
`spy` is an observational scatter-gather query that accepts underconstrained
selectors and returns metadata or collections of existing artifacts.

- **Routing (`Promise.allSettled`):** Unlike `read` which races neighbors
  (`Promise.any`), `spy` queries every neighbor, waits for all responses, and
  merges them.
- **Explicit Constraints:** Missing keys in a `spy` parameter object do NOT
  imply a wildcard (to avoid infinite schema guessing). Instead, queries must
  use explicit constraint values or objects (e.g., `{"width": {"$gt": 10}}` or
  `{"node": "*"}`).

### 6.3 The VFS Bundle Multiplexer

Because a `spy` query can return multiple results from multiple peers, the mesh
must aggregate them efficiently.

- **Format:** The VFS uses the native VFS bundle format
  (`\n=<LEN> <NAME>\n<CONTENT>`) to multiplex responses.
- **Canonical Identity Header:** The `<NAME>` field of each VFS entry in a `spy`
  response MUST be the stringified JSON of the **Canonical Selector** for that
  payload.
- **Blind Streaming:** Intermediate nodes (`MeshLink`) do not need to parse or
  merge JSON arrays in memory. They simply pipe incoming VFS streams
  sequentially into the outgoing response stream, allowing infinite-length
  discovery responses with zero memory overhead.
- **Deduplication:** The ultimate requester (e.g., the UI) can read the incoming
  Canonical Selector headers to deduplicate payloads on the fly before consuming
  the `<LEN>` bytes.

## 7. Mesh Observability & Pub-Sub

The VFS implements a **Demand-Driven Pub-Sub** architecture to provide real-time
observability of the mesh without central coordination.

### 7.1 Ephemeral Interest (SUB)

Nodes express interest in specific topics (Selectors or Path Patterns) by
sending a `SUB` command to their neighbors.

- **Hop-by-Hop Routing:** A node does NOT record the original requester's
  identity. It only records that a **specific neighbor** is interested in a
  topic.
- **Interest Table:** Each node maintains a local mapping:
  `Topic -> Set<NeighborID>`.
- **Lease-Based (TTL):** Every subscription includes an `expiresAt` timestamp.
  If not renewed, the interest is automatically pruned to prevent "Event Storms"
  for stale requests.

### 7.2 Publication & Notification (PUB)

When an event occurs (e.g., `MATH_START`, `DISK_WRITE`, `ERROR`), the node
**publishes** a notification.

1.  **Local Match:** The node checks its Interest Table for any neighbors
    subscribed to the event's topic (or matching glob patterns).
2.  **Broadcast:** The notification is sent to matching neighbors.
3.  **Recursive Propagation:** Neighbors repeat this process, forwarding the
    notification down the "Interest Tree" until it reaches the ultimate
    subscribers (e.g., the Browser UI).

### 7.3 Observability Topics

Topics are formatted as VFS Selectors or Glob-like Path patterns:

- **Specific:** `shape/hexagon/full?radius=30` (Status of a unique calculation).
- **Pattern:** `shape/hexagon/*` (Any activity related to hexagons).
- **System:** `sys/peers` (Mesh topology and reachability updates).

### 7.4 Protocol Integration

- **Forward Peers:** Notifications are sent via a symmetric `POST /notify` or
  persistent SSE/WebSocket channel.
- **Reverse Peers:** Notifications are pushed as commands down the active
  `POST /listen` long-poll response.

## 8. Operation Interface Model (Ports)

VFS Operations are modeled as **Port Maps** where the schema defines the
relationship between Inputs, Arguments, and Outputs.

### 8.1 Symmetrical Port Mapping

- **`arguments`**: The unified set of all parameter values (literals and
  selectors).
- **`inputs`**: A dictionary mapping argument keys to their role as VFS-linked
  sources.
- **`outputs`**: A dictionary mapping result ports to their semantic roles and
  MIME types.

### 8.2 Argument-Output Overlap (Sinks)

A side-effect is modeled by an overlap between the `arguments` and `outputs`
keys. If a key (e.g., `path`) exists in both, the value of that argument
determines the VFS destination where the produced data will be "sunk".

### 8.3 Aliases & Identity (The Pass-Through)

`metadata.aliases: { "$out": "$in" }` asserts that the **data content** of
`$out` is identical to `$in`. This allows operations like `op/pdf` to function
as logical "Tees".

### 8.4 Example: Side-Effector (`op/pdf`)

```json
{
  "path": "op/pdf",
  "arguments": {
    "$in": { "type": "shape" },
    "$out": { "type": "shape" },
    "path": { "type": "string", "default": "export.pdf" }
  },
  "inputs": {
    "$in": { "type": "shape" }
  },
  "outputs": {
    "$out": { "type": "shape" },
    "path": { "mime": "application/pdf" }
  },
  "metadata": {
    "aliases": { "$out": "$in" }
  }
}
```

## 9. Core Type System

To ensure structural integrity across the mesh, the VFS distinguishes between
raw spatial data and semantic organizational containers.

### 9.1 Geometry Type (`geometry`)

- **Nature:** Raw spatial payload (text/JOT).
- **Content:** Mesh data (Vertices, Edges, Faces).
- **Identity:** Typically content-addressed under `geo/mesh/*`.
- **Constraint:** Pure data; has no concept of transformation or metadata.
- **Indexing**: Uses **0-based vertex indexing** for all primitive codes (`f`, `t`, `s`, `p`) to enable direct memory mapping in high-performance kernels.

### 9.2 Shape Type (`shape`)

- **Nature:** Semantic container (JSON).
- **Contract**: Operators are **Higher-Order Providers**. They must:
  1. **Read Shape**: Accept a Shape JSON as input.
  2. **Unwrap**: Recursively resolve the `geometry` selector within the shape.
  3. **Transform**: Apply the `tf` (4x4 matrix) to vertices to reach world-space.
  4. **Process & Re-package**: Generate new geometry and return a *new* Shape JSON.
- **Structure:**
  - `geometry`: A structured **VFS Selector** (e.g. `{ "path": "geo/mesh", "parameters": {"cid": "..."}}`) pointing to a `geometry` artifact.
  - `tf`: Affine transformation matrix (16 doubles).
  - `tags`: Key-value tags (color, name, material).
  - `components`: Nested `shape` objects for hierarchical assemblies.

### 9.3 Type-Specific Port Mapping

Operations use these types in their `arguments` and `outputs` blocks to define
their contract:

- `shape/box` produces a `shape`.
- `op/points` consumes a `shape` but produces `geometry`.
- `op/offset` consumes and produces a `shape`.
