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

## 3. Peer-to-Peer Protocol (Routing)

### 3.1 Identity Introduction (`POST /register`)

The VFS decouples transport connections from peer identities. On connection establishment, a node MUST send a `POST /register` to its neighbor.
- **Format:** Supports both JSON body (`{"id": "peer-id", "url": "http://..."}`) and URL query parameters (`?peerId=...&url=...`).
- **Response:** `{ "id": "local-id", "reachability": "DIRECT|REVERSE" }`. 
  - **REVERSE Signal:** If a node returns `REVERSE`, the requester MUST initiate a polling loop via `POST /listen` to receive incoming mesh commands.

### 3.2 Peer Connection Abstraction

All peer interactions are managed through a unified **Connection** abstraction.
- **ForwardConnection:** Used for `DIRECT` reachability. Delivers publications and requests immediately via outbound HTTP `POST` calls.
- **ReverseConnection:** Used for `REVERSE` reachability (e.g., Browsers).
  - **Encapsulated Responsibility:** The `ReverseConnection` class manages BOTH the server-side listener pool and the client-side active polling loop.
  - **409 Conflict Protection:** Servers MUST reject a `POST /listen` with a **409 Conflict** if a poll is already active for that peer ID. Clients MUST treat a 409 as a critical failure (zombie process detection).

### 3.3 Discovery & Lifecycle (`sys/`)

The VFS uses specialized system paths for mesh management.

- **Topology Updates (`sys/topo`):** Nodes notify neighbors of topology changes.
  - Content: `{ peer: "id", neighbors: [ { "id": "peer-id", "reachability": "DIRECT|REVERSE", "protocol": "HTTP|REVERSE-HTTP|WS" } ] }`
- **Schema Discovery (`sys/schema`):** Providers announce their available operators.
  - Content: `{ provider: "id", catalog: { "path": schema } }`
  - Trigger: Nodes MUST send this notification immediately upon `POST /register` completion or dynamic operator creation.

### 3.4 Interest Forwarding (`POST /subscribe`)

To support reactive workflows (e.g., interactive boolean previews), nodes propagate "Interests" across the mesh.

- **Subscription Request:** `POST /subscribe`
  - Content: `{ selector, expiresAt, stack }`
- **Mesh Painting:** When a node receives a subscription, it MUST:
  1.  Record the interest locally.
  2.  Forward the subscription to all OTHER connected peers (omitting any peer in the `stack` to prevent cycles).
  3.  Immediately `NOTIFY` the subscriber if the data already exists locally.
- **Clean Cleanup:** Subscriptions expire automatically after `expiresAt`.

### 3.5 Browser Compatibility & CORS

Native VFS nodes intended for browser use MUST support Cross-Origin Resource Sharing (CORS).
- **Required Headers:** `Access-Control-Allow-Origin`, `Access-Control-Allow-Methods`, `Access-Control-Allow-Headers`.
- **Allowed Headers:** MUST include `Content-Type`, `X-VFS-Peer-Id`, `X-VFS-Reply-To`, and `X-VFS-Id`.
- **Preflight:** Nodes MUST handle `OPTIONS` requests for all mesh routes.

### 3.4 Reverse Link Polling (`POST /listen`)

Peers without a stable incoming URL (e.g., Browsers) receive mesh events by polling the `/listen` endpoint of their neighbors.
- **Endpoint:** `POST /listen`
- **Headers:** `x-vfs-peer-id: <local-id>`
- **Response:** 
  - `200 OK`: Returns a single JSON Command object (e.g., `NOTIFY`, `READ`).
  - `204 No Content`: No events pending (returned ONLY after a timeout).
- **Long-Polling Contract:** Servers MUST NOT return `204` immediately if the queue is empty. They must wait for a publication or a timeout (e.g., 30s).

### 3.5 Formal Links (Unambiguous Redirection)

The VFS supports **Formal Links**, a mechanism for redirecting one Selector to another. 

- **Link Definition:** A Link is an artifact whose content is another address (a Selector). 
- **Storage:**
  - **Metadata (`.meta`)**: MUST contain `state: "AVAILABLE"` and `encoding: "link"`.
  - **Data (`.data`)**: MUST contain the **Target Selector** serialized as JSON (or JCB).
- **Redundancy Removal:** The legacy `vfs:/` URI prefix is prohibited. The `encoding: "link"` metadata flag is the unambiguous signal that the data payload should be interpreted as a target identity for re-resolution.
- **Resolution Behavior:** When the VFS internal resolution (`_readResult`) encounters `encoding: "link"`, it MUST read the data payload, hydrate it into a formal `Selector`, and recursively resolve that Selector.
- **Cycle Protection:** Nodes MUST maintain a `resolutionStack` of CIDs encountered during a single resolution chain. If a CID is encountered that is already in the stack, the resolution MUST fail with a "Circular Link Detected" error.
- **Owner Sovereignty:** A Link is an independent entry owned by its source Selector. If the source Selector entry is deleted, the link is destroyed, but the target artifact remains unaffected.

### 3.6 Recursive Bread-crumb READ (`POST /read`)

The `read` operation is the primary mechanism for demand-driven data retrieval.

- **Atomic Wire Format:** Requests MUST wrap the target Selector or CID in a top-level key.
  ```json
  {
    "selector": { "path": "jot/Box", "parameters": { "width": 10 }, "output": "file" },
    "expiresAt": 1700000000000,
    "stack": ["node-A"]
  }
  ```
  Or for direct CID retrieval:
  ```json
  {
    "cid": "deadbeef1234567890...",
    "expiresAt": 1700000000000,
    "stack": ["node-A"]
  }
  ```
- **TTL Enforcement:** Nodes MUST verify `expiresAt` (milliseconds epoch). If the current time exceeds `expiresAt`, the request MUST be rejected with a `404` or `410` status. The default TTL for requests without an explicit `expiresAt` is **30 seconds**.

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
