# Decentralized Mesh-VFS: Implementation Record

## 1. Summary

The JotCAD VFS has been transformed from a Centralized Hub architecture to a
**Decentralized Mesh-VFS**. This transition eliminates the Hub as a Single Point
of Failure (SPOF) and resolves systemic metadata corruption by enforcing local
sovereignty and deterministic identity.

## 2. Core Principles

### 2.1. Local Sovereignty & Ephemeral Lifecycle

- **Isolated Storage:** Every node maintains its own independent `.vfs_storage`
  directory. No state is shared via common disk or database.
- **Clean Slate (Immunity):** On startup, every node physically wipes its local
  storage. This ensures immunity to partial writes from previous sessions.
- **Local-First Writes:** `vfs.write` commits data immediately and only to the
  node's local disk.

### 2.2. Literal Identity (The Universal CID)

The JotCAD mesh uses a strict **Content Identifier (CID)** model for all addressing.

- **Computation-Addressed Shapes:** Every request is standardized into a Selector object `{path, parameters, output}`. We hash this entire object to create the CID for the Shape or artifact.
- **Content-Addressed Geometry:** The heavy binary geometry itself is hashed directly from its raw bytes to produce its CID. This ensures perfect deduplication.
- **Flat Architecture:** The VFS acts as a pure key-value store mapping `CID -> .data`. Formal `.meta` links are used to handle semantic aliasing between different Selectors.

### 2.3. No Semantic Magic (Literal Mesh)

Normalization is strictly structural. We do not reorder original objects or fix
user parameters (e.g., radius to diameter) during the hashing phase.
- **Literal Hashing:** If a user asks for `radius`, they get a hash for `radius`.
- **Semantic Aliasing:** Operators (Actors) use **Formal VFS Links** to bridge
  different request styles (the "remainer") to canonical content.
- **Deterministic Identity:** Both C++ and JavaScript utilize the **Safe-JCB
  (Base64-encoded JCB)** format for hashing and storage. This ensures
  byte-for-byte identity across languages without JSON serialization
  discrepancies or ASCII escaping issues.

### 2.4. Demand-Driven Workflow (Silent Mesh)

- **No Proactive Gossip:** The mesh remains silent until a `READ` is initiated.
  There are no background state syncs.
- **Implicit Trigger:** Requesting data is the sole trigger for work. If a
  provider doesn't have the result, it starts the calculation and holds the
  connection open until completion.

## 3. Protocol Details

### 3.1. The Synchronous `READ` (`POST /read`)

Requests flow through the mesh using a synchronous bread-crumb stack:

1.  **Requester:** Sends `POST /read` with a `path`, `parameters`, and a `stack`
    containing its own ID.
2.  **Intermediate:**
    - **Loop Prevention:** Checks if its own ID is in the `stack`; if so, it
      returns 404.
    - **Auto-Peering:** Learns the requester's URL from the `x-vfs-local-url`
      header and adds it to its neighbors list.
    - **Parallel Search:** Forwards the request to all neighbors NOT in the
      stack.
3.  **Leaf:** Provisionally generates the data, writes it to local storage, and
    streams it back.
4.  **Fulfillment:** Bytes flow back through the bread-crumb chain. Intermediate
    nodes cache data locally as it passes through.

### 3.2. Technical Standards

- **Web Stream Unification:** Data transfer is standardized on the Web Streams
  API.
- **Parallel Race:** Nodes use `Promise.any` to return the first successful
  response from neighbors.

### 3.3. Local Orchestration (The Worker Bridge)

While Global SSE is decommissioned, nodes maintain a **Local-Only SSE** loop for
their own internal workers (C++ engines, exporters, etc.) to satisfy the
**Demand-Driven** mandate.

- **Local SSE (`/watch`):** Private channel for a node to talk to its own local
  workers.
- **State Injection (`receive`):** Internal components signal lifecycle states
  (e.g., `LISTENING`) locally without mesh broadcast.

### 3.4. Request Deduplication (Atomic Reservation)

To prevent redundant CPU/Network usage, nodes implement an **Atomic
Reservation** pattern:

- **Synchronous Locking:** Upon receiving a `READ`, the node synchronously
  checks an `activeWait` registry for the specific Selector.
- **Task Joining:** If a task is in progress, the requester `awaits` the
  existing work promise rather than starting a new one.
- **Guaranteed Fulfillment:** The work promise MUST NOT resolve until the data
  is fully committed to local storage (`await vfs.write`). This prevents race
  conditions where secondary requesters wake up before the data is physically
  available.
- **Stream Sovereignty:** Once the work promise resolves, every requester
  receives a **fresh, independent stream** from local storage.

### 3.5. Request Expiration (Safe TTL)

To prevent "zombie" requests from clogging the mesh:

- **expiresAt Timestamp:** Every `POST /read` includes a millisecond epoch
  timestamp.
- **Protocol Enforcement:** Nodes check this timestamp before processing. If
  `now > expiresAt`, the node returns `408 Request Expired`.
- **Default TTL:** Nodes default to a **30-second TTL** for discovery.

## 4. Node Types & Registry

The system utilizes two primary node implementations:

- **C++ Native Node (`bin/ops`):** High-performance geometry provisioning using
  CGAL and `httplib`.
- **Node.js Native Node:** Orchestration and Export services using `VFS` and
  `MeshLink` classes.

## 5. Deployment Topology

The system is managed by `orchestrator.js` using a symmetric peering structure:

- **Ops Node (9091):** C++ leaf provider for geometry.
- **Export Node (9092):** Node.js gateway peered with Ops; handles PDF
  generation and file exports.
- **UX (Browser):** Node.js client node peered with the Export Node.

## 6. Observability Mandate (Pub-Sub)

To provide real-time visibility into the demand-driven execution, the mesh
implements a **Hop-by-Hop Pub-Sub** system.

### 6.1. Subscription Propagation (SUB)

- **Interest Table:** Nodes maintain a local table of
  `Topic -> Set<NeighborID>`.
- **Demand-Linked:** Subscriptions are typically created automatically when a
  `READ` is initiated.
- **Expiry:** All subscriptions are ephemeral and lease-based.

### 6.2. Publication Propagation (PUB)

- **Local Events:** Nodes emit local events for lifecycle transitions
  (`MATH_START`, `DISK_WRITE`).
- **Gossip Forwarding:** Notifications are broadcast only to neighbors with
  matching subscriptions.
- **Visual Feedback:** The UX node aggregates these notifications to draw a
  live, pulsing graph of the mesh activity.

## 7. Implementation Status (Updated April 2026)

| Feature                  | Status | Details                                                |
| :----------------------- | :----- | :----------------------------------------------------- |
| **Content-Addressing**   | [DONE] | Pure separation between Selector Key and Content CID.  |
| **JCB Protocol Sync**    | [DONE] | Cross-language identity via Canonical Binary hashing.  |
| **Actor Fulfillment**    | [DONE] | Operators satisfy addresses via mandatory mesh writes. |
| **Fail-Fast Validation** | [DONE] | Synchronous Schema enforcement in JS/C++.              |
| **TTL Enforcement**      | [DONE] | Request expiration verified across mesh nodes.         |
| **Stream Integrity**     | [DONE] | Partial/Corrupted stream detection via Content-Length. |
| **Handshake Protocol**   | [DONE] | Symmetric peer discovery and reachability negotiation. |
| **Mesh Pulse (Pub-Sub)** | [DONE] | Event-driven notifications with long-poll pooling.    |
| **Formal Links**         | [DONE] | Recursive alias resolution for semantic standardization.|

## 8. Aspirational Goals

### 8.1. Resource-Constrained Nodes (ESP/Microcontrollers)

A primary long-term goal is to extend the JotCAD mesh to **Microcontroller-based
nodes** (e.g., ESP32).

- **Footprint:** The VFS and MeshLink logic must be optimized for extremely low
  memory (RAM) and limited flash storage.
- **Protocol Efficiency:** Implementation of a "Tiny Mesh" subset of the
  protocol, potentially utilizing binary serialization or stripped-down HTTP to
  minimize overhead.
- **Hardware Integration:** Allowing hardware-level devices (CNC controllers,
  sensors) to act as leaf providers or observers within the geometric mesh.
