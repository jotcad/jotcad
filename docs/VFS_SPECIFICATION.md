# JotCAD Virtual File System (VFS) Specification: Decentralized Mesh

The JotCAD VFS is a distributed, content-addressable mesh system designed for
collaborative geometry processing across C++, Node.js, and Browser environments.
It uses a **Decentralized Peer-to-Peer** architecture where participants
coordinate through recursive bread-crumb routing and local content-addressable
storage.

## 1. Identity & Addressing

The VFS uses a **Symmetric Identity** model where everything is addressed by a **CID (Content Identifier)**.

### 1.1 The CID and The Selector

The global identity of any artifact is its **CID** (a SHA-256 hash). However, how that CID is generated depends on the artifact type:

1. **Geometry (Content-Addressed):** The CID is the hash of the raw mathematical bytes.
2. **Shapes & Artifacts (Computation-Addressed):** The CID is the hash of the **Selector** that produced it.

A **Selector** is a recomposable request object containing:
- `path` (e.g., `jot/Hexagon/diameter`)
- `parameters` (e.g., `{"diameter": 30}`)
- `output` (e.g., `"thumb"` or `"$out"`. If omitted, it targets the operation itself).

- **Formal Identity & Sovereignty Mandate:** 
  - **Unique Per-Session IDs:** Every mesh participant (including browser tabs) MUST maintain a unique Peer ID. Sharing IDs between concurrent instances (zombies, multiple tabs) is a protocol violation.
  - **Identity Stability:** Clients SHOULD use `sessionStorage` to maintain ID stability across soft-refreshes (Vite HMR) while ensuring ID uniqueness across distinct tabs.
  - **No Implicit Linkage:** A base Selector (without an `output` port) represents the **Operation Identity**, not its result. It is a terminal error for the VFS to automatically resolve a base selector to its `$out` port.
  - **Explicit Port Targeting:** Callers (Compilers, Tests, other Operators) MUST explicitly append the target port (e.g., `:$out`) to retrieve computational results.

- **Atomic Address:** The Selector is treated as an atomic unit. API methods consume the entire object (e.g., `vfs.read(selector)`). Hashing the *entire* Selector (including the `output` field) yields the CID for that specific artifact. **Deconstructing a Selector into top-level keys in metadata or network messages is strictly prohibited.**
- **Parametric Standardization:** Parameters MUST be normalized (e.g., radial/apothem parameters to `diameter` or symmetric `Interval` objects) before execution to ensure deterministic CIDs.
- **Pure Router Model**: The VFS is a pure routing layer. It does not interpret or decode data payloads (JSON, Text, Binary) except when required for routing (e.g., following links).
- **Strict read Protocol**: `vfs.read(target)` returns a Promise resolving to `{ stream: ReadableStream, metadata: Object }`. It is the **caller's** responsibility to consume the stream and decode it (e.g., using `TextDecoder` or `JSON.parse`) based on the `metadata.encoding` field.
- **Secure Context (WebCrypto):** The VFS requires the WebCrypto API for hashing. Because browsers restrict `crypto.subtle` to Secure Contexts, the VFS MUST throw a descriptive error if `crypto.subtle` is undefined, notifying the user that HTTPS is required for non-localhost environments.

### 1.2 Network Transmission & Hydration

CIDs CAN and SHOULD be transmitted safely over the mesh network. Nodes can request data directly by CID, or they can request the execution of a computation by sending a Selector.

- **Strict Structural Normalization:** To ensure deterministic CIDs across environments, Selectors MUST be normalized before hashing.
  - `parameters` MUST always be present (defaulting to `{}`).
  - `output` MUST be omitted if empty.
  - Object keys MUST be sorted alphabetically during binary encoding (JCB).
- **Boundary Hydration:** Network handlers (REST, WebSockets) and UI entry points MUST explicitly hydrate incoming JSON representations into formal `Selector` instances using `Selector.fromObject()` (JS) or `Selector::from_json()` (C++) before passing them to the VFS core.
- **Context-Safe Identification:** To prevent "identity drift" across bundlers (Vite) or cross-window contexts (Puppeteer), string-like CIDs MUST be identified using robust detection (e.g., `Object.prototype.toString.call(val) === '[object String]'`).

## 2. Actor Fulfillment Protocol

JotCAD operators follow a strict **Actor Model** for mesh fulfillment.

### 2.1 The Fulfillment Contract (Void Fulfillment)

Every operator call is assigned a unique address (the `fulfilling` Selector). 
The operator's sole responsibility is to **Fulfill** this address by ensuring the data exists in local storage at the requested identity.

- **Non-Returning Handlers:** Handlers in the VFS MUST return `void`. They signify success by finishing execution and failure by throwing an exception.
- **Fulfillment over Data-Piping:** Operators write their results directly to the VFS. The VFS core then handles the terminal data retrieval. This eliminates redundant byte-copying and ensures the VFS remains the sole arbiter of data state.

### 2.2 Immutable Content (CID Protection)

Input artifacts are strictly read-only. Transformative operators (e.g., `cut`, `offset`) MUST NOT "update" the CID-addressed geometry of their inputs. Instead:
1.  **Read:** The operator reads the input geometry from its CID.
2.  **Calculate:** The operator performs the transformation.
3.  **Materialize:** The operator writes the NEW geometry to the mesh using **Content Addressing** (`vfs->materialize(res_geo)`), which returns a new CID.
4.  **Reference:** The final result `Shape` embeds this new geometry CID.
5.  **Fulfill:** The `Shape` is written to the operator's assigned `fulfilling` address.

### 2.3 Flat Port Architecture

There are NO "Partitioned Output Ports" or `ports` maps in `.meta` files.
Every distinct output port (e.g., `$out`, `file`, `thumb`) is just a field in the Selector. Because the `output` field is hashed along with the path and parameters, every artifact gets its own unique, deterministic CID and its own discrete `.data` file.

### 3. Network & Routing Protocol (Eclipse Zenoh)

The VFS peer-to-peer network is built natively on **Eclipse Zenoh**, replacing custom WebSocket and HTTP routing layers with Zenoh's decentralized sessions, queryables, and pub-sub engine.

### 3.1 Peer Sessions & Discovery
* **Peer Sovereignty:** Every mesh participant maintains a unique Peer ID. Zenoh sessions establish direct or routed peer-to-peer connections automatically.
* **Ephemeral Lifecycles:** When a node (such as a browser tab) disconnects, Zenoh's session keepalive detects the loss, automatically tearing down all queryables, subscriptions, and associated peer entries without manual timeout loops.

### 3.2 Key Expression Mapping
VFS addresses are translated directly into Zenoh key expressions using a strict path layout to preserve the **Identity Duality** between computational recipes (Selectors) and content:

1. **Generative (Selector) Path:** `jot/vfs/op/<OperatorName>`
   * Used for computational execution requests.
   * Parameter representation: Appended as standard URL-encoded query parameters: `jot/vfs/op/Cut?subject=CID_BOX&tool=CID_SPHERE`
2. **Non-Generative (Content) Path:** `jot/vfs/cid/<CID>`
   * Used for direct retrieval of static content-addressed buffers (geometry or shape JSONs).
   * Parameter representation: None.

### 3.3 Actor Fulfillment (Queryables)
* **On-Demand Processing:** Operator nodes register a Zenoh queryable on `jot/vfs/op/**`.
* **Fulfillment on Cache Miss:** When a queryable receives a `z_get` request, it checks the local cache. If a cache miss occurs, the operator runs the geometry kernel, writes the result to local storage, and replies via `z_query_reply`.
* **Non-Generative CID Fetching:** Queries on `jot/vfs/cid/**` are strictly non-generative. A cache miss on a content path is resolved by searching the mesh or waiting; it must *never* trigger compilation or operator execution.

### 3.4 Request Deduplication (activeWait Task-Joining)
To prevent redundant execution and CPU/Network bloat when duplicate queries for the same `Selector` arrive concurrently before the cache commits:
* The VFS maintains a local `activeWait` registry mapping in-progress Selector hashes to active computation promises.
* Subsequent concurrent queries for the same Selector will join the existing promise instead of spawning a new compiler/CGAL process.
* Once the initial computation finishes and commits to local disk, the promise resolves and replies to all waiting queries concurrently.

### 3.5 Failure Propagation & Zombie Prevention
To prevent client queries and mesh threads from blocking indefinitely on failed compilations or operator exceptions:
* **Active Error Replies:** The queryable handler wraps execution in a `try/catch` block. On exception, it dispatches an immediate error payload (status `500` JSON containing the error description) via `z_query_reply`.
* **Registry Cleanup:** The provider must instantly remove the Selector CID from the local `activeWait` map and reject the master promise, waking up and rejecting all joined client queries.
* **Native Timeouts:** Requesters configure a strict query timeout on `z_get` calls (default `10 seconds`). If a provider crashes silently, the requester's Zenoh engine automatically triggers a timeout rejection.

### 3.6 Formal Links (Unambiguous Redirection)
The VFS supports metadata-driven redirection:
* **Link Definition:** An entry with `encoding: "link"` in its metadata. The data payload contains the serialized target `Selector`.
* **Resolution Behavior:** When `_readResult` encounters `encoding: "link"`, it recursively resolves the target Selector.
* **Cycle Protection:** A `resolutionStack` of CIDs is maintained during resolution. Encountering a duplicate CID rejects the request with a "Circular Link Detected" error.

### 3.7 TypeScript Client & Remote-API Bridging
Because JavaScript/TypeScript client environments (like web browsers or Node.js running `@eclipse-zenoh/zenoh-ts`) cannot run the native Rust/C++ routing engine directly and rely on WebSockets:
* **Protocol Bridging Requirement:** JS clients MUST connect exclusively to a Zenoh Router running the `remote_api` plugin (e.g., `zenoh-bridge-remote-api`). Direct WebSocket connection attempts from `zenoh-ts` to raw native C++ Zenoh-C node listeners fail due to protocol mismatches (e.g., triggering `unexpected message type 3`).
* **Router Port Structure:** The Zenoh Router listens for native TCP peers/routers on port `N` (e.g. `9200`) and hosts the WebSocket remote-api plugin/bridge on port `N + 1000` (e.g. `10200`).
* **MeshLink Locator Translation:** The JavaScript VFS `MeshLink` automatically maps client HTTP/HTTPS neighbor URLs (e.g., `http://localhost:9200`) to the corresponding WebSocket Remote-API bridge locator (e.g., `ws://localhost:10200`) during initialization.
* **Unified Mesh Topology:** Native C++ nodes connect to the Zenoh Router via TCP (`tcp/127.0.0.1:9200`), routing queries and content lookups transparently between C++ and JS over the shared session.

### 3.8 Catalog Discovery & Notification Propagation
To maintain unified decentralized routing lists and enable UX interceptors to accurately track newly registered user operations:
* **All-Target Querying (target: 1):** Discovery query actions on `jot/vfs/catalog` MUST explicitly pass `target: 1` (`QueryTarget.ALL`) to the Zenoh `session.get` call. Using the default target (`QueryTarget.BEST_MATCHING`) causes Zenoh to only route the query to a single "best-matching" node (typically the closest native C++ peer), ignoring all other nodes' catalogs.
* **UX Interceptor Integration:** The client UX layer (via SolidJS) overrides and intercepts `mesh.notify(selector, payload, stack)` to monitor schemas and update frontend states. Incoming Zenoh sub/pub notification updates MUST be routed through `this.notify(selector, payload, ['incoming'])` rather than using direct event emitter calls, allowing the UX state machine to populate operations without entering infinite network loops.

### 3.9 Multicast Domain & Mesh Isolation (Broadcast Separation)
To prevent discovery cross-talk and data leaks between concurrent meshes running on the same local network or subnet:
* **Custom Multicast Port Configuration:** By default, Zenoh nodes scout on UDP multicast group `224.0.0.224:7446`. To run distinct meshes (e.g. `test/standard` vs `live/webcam`), each profile MUST use a unique multicast group address/port.
* **Environment Overrides:** The orchestrator and the C++ nodes dynamically support the `ZENOH_MULTICAST_ADDRESS` environment variable (e.g., `224.0.0.224:7447`).
* **Secure by Default:** If `ZENOH_MULTICAST_ADDRESS` is not defined in the environment, the VFS node MUST default to disabling multicast scouting entirely (`"scouting": {"multicast": {"enabled": false}}`), relying strictly on explicit TCP connection endpoints (unicast) for its network sessions.

### 3.10 Query Target & Consolidation
To prevent query deadlocks and ensure correct data resolution in a distributed P2P mesh:
* **Mesh-Wide Query Targeting (`QueryTarget::ALL`):** Content CID queries (`jot/vfs/cid/<CID>`) and shared catalog queries (`jot/vfs/catalog`) MUST be queried with `ALL` targeting. This guarantees that queries are routed to all matching queryables on the mesh instead of terminating early on the first responder (which may return a `404`).
* **Disabled Consolidation (`ConsolidationMode::NONE`):** To prevent Zenoh routers from merging and discarding replies from multiple matching nodes, CID and catalog queries MUST disable consolidation. This ensures that the client receives all individual node responses, allowing it to find the owner's `200` reply in the presence of `404` responses from non-owners.
* **Clamped Selector Cache Lookups:** Before evaluating a Selector, clients check if its output is already cached on the mesh by querying its CID. Since this cache lookup results in a miss for new computations, to prevent Zenoh's mesh-wide `ALL` query from blocking for the full timeout (defaulting to `2s`), Selector cache lookups MUST clamp their timeout (e.g., `200ms`). This enables rapid fallback to operator evaluation.

### 3.11 Root-Prefixed Routing Layout (Machine-Centric Addressing)
To enable physical machine isolation and avoid double-prefixing of key expressions:
* **Key Expression Structure:** Generative routes and notifications are prefixed with the routing machine's identifier:
  * Operator Queryables: `<machine-id>/jot/vfs/op/<path>`
  * Published/Notified Topics: `<machine-id>/jot/vfs/pub/<path>`
* **Dynamic Prefix Stripping:** Upon receiving a query, C++ and JS VFS servers MUST dynamically extract the logical operator path by finding and stripping `/jot/vfs/op/` (and `/jot/vfs/pub/` for subscribers), ensuring compatibility with wildcard requests (`*/jot/vfs/op/...`).

### 3.12 Concurrency Gate & Alphabetical Spillover Load Balancer
To prevent overloading active nodes and provide robust failover:
* **Concurrency Gate:** Every VFS Node tracks concurrent executions using an atomic counter (`active_ops_count_`) capped at a configured limit (`max_concurrent_ops_`). If the limit is reached, query handlers reject queries immediately with a `503 Server Busy` status payload.
* **Alphabetical Spillover Loop:** Clients discover available nodes in the mesh via system metrics subscriptions (`*/jot/vfs/pub/jot/vfs/metrics/system/**`), sorting active machine IDs alphabetically. Clients query them sequentially. If a target is busy locally or returns a `503` or `429` rejection over Zenoh, the client prints a spillover log and falls back to the next target in the alphabetical list.
* **Local Memory Cache Bypass:** To avoid routing delays on locally published pub/sub topics, VFS nodes check their memory cache (`local_cache_`/`local_cache_binary_`) at the start of local selector reads, resolving local queries instantly.

## 4. Core Type System

### 4.1 Geometry Type (`geometry`)

- **Nature:** Raw spatial payload.
- **Addressing:** Exclusively Content-Addressed (CID = hash(data)).
- **Kernel:** All C++ math MUST use the **CGAL Exact Kernel (`FT`)**. 

### 4.2 Shape Type (`shape`)

- **Nature:** Semantic container (JSON).
- **Addressing:** Computation-Addressed (CID = hash(Selector)).
- **Structure:**
  - `geometry`: An optional **CID** (strictly a CID string, *never* a Selector).
  - `tf`: 4x4 Affine transformation matrix. Stored as an array of 16 **Exact Ratio Strings** (`"n/d"`) to ensure bit-exact identity stability across platforms.
  - `tags`: Key-value metadata.
  - `components`: Nested `shape` objects.
