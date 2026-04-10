# JotCAD Virtual File System (VFS) Specification

The JotCAD VFS is a distributed, content-addressable blackboard system designed for collaborative geometry processing across C++, Node.js, and Browser environments. It uses a **Symmetric Peer-to-Hub** architecture where all participants share a unified, eventually-consistent view of the world.

## 1. Identity & Addressing

The VFS distinguishes between **Wire Identity** (used for routing and coordination) and **Storage Identity** (used for local caching).

### 1.1 The Selector (Wire Identity)
The global identity of any artifact is its **Selector**: a combination of a `path` (e.g., `shape/hexagon`) and a `parameters` object (e.g., `{"radius": 50}`).
- **Normalization:** Selectors are deeply and recursively normalized. Parameters are sorted alphabetically by key, and nested objects are themselves normalized.
- **Matching:** Peer coordination uses **Logical Matching** (deep equality) of selectors, ensuring that different environments (C++, JS) can collaborate even if their JSON serialization differs slightly.

### 1.2 The Content-ID (CID) (Storage Identity)
The **CID** is a local storage key derived from the normalized selector. 
- **Hash:** SHA-256 of `path + JSON.stringify(normalizedParameters)`.
- **Scope:** CIDs are used for internal indexing and disk storage. On the wire, the Hub strips CIDs to prevent "Hash Drift" from blocking communication.

## 2. States & Lifecycle

A coordinate (`path` + `parameters`) can be in one of the following states:

- **LISTENING:** (Priority 0) One or more peers are capable of fulfilling this demand.
- **PENDING:** (Priority 1) Demand exists (someone called `read` or `tickle`), but no data is available.
- **PROVISIONING:** (Priority 2) An agent has "leased" the coordinate and is currently computing the result.
- **AVAILABLE:** (Priority 3) The result is computed and stored in the VFS.
- **LINKED:** (Priority 3) An alias pointing to another coordinate.
- **SCHEMA:** (Priority 4) A special state for path-level metadata (defined at `path@schema`).

### Multi-Source Identity
Each state entry tracks a `sources` array of Peer IDs (e.g., `peer:geo-ops-service`, `node`). This prevents different peers from overwriting the same coordinate metadata.

## 3. Virtual Mailbox Protocol (Routing)

The VFS Hub acts as a **Logical Router** for peers that are behind firewalls or cannot accept incoming HTTP connections (like private C++ services or browser workers).

### 3.1 Demand Routing
When a peer requests a coordinate that is currently in a `LISTENING` state but not `AVAILABLE`, the Hub performs **Demand Routing**:
1.  **Detection:** The Hub identifies peers `LISTENING` for the specific **Path**.
2.  **Delivery:** The Hub sends a `COMMAND:READ` message over the persistent SSE (`/vfs/watch`) connection of the listening peer.
3.  **Fulfillment:** The peer receives the command, computes the result, and sends it back to the Hub via `POST /vfs/reply/:commandId`.
4.  **Persistence (The Blackboard):** The Hub "Tees" the fulfillment stream—it pipes the data to the requester while simultaneously saving it to its own VFS (transitioning the state to `AVAILABLE`).
5.  **Completion:** Subsequent requests for the same selector are served directly from the Hub's cache without triggering new demands.

### 3.2 Robust Reprocessing
The Hub maintains an **Unfulfilled Demands** queue. It automatically re-evaluates and routes these demands whenever a new peer connects or a new `LISTENING` capability is advertised.

## 4. Environment Robustness

The VFS is designed to be **Cross-Environment Aware**:
- **Stream Handling:** Internal storage and REST bridges automatically detect and handle both **Web Streams** (browser) and **Node.js Readable Streams**, ensuring non-blocking data flow across all environments.
- **Recursive Resolution:** C++ operators and Node services correctly resolve nested `vfs:/` URIs by recursively querying the VFS blackboard.

## 5. API Patterns

### Structured Selectors
To avoid complex URI encoding in nested pipelines, parameters can be passed as **Structured Selectors**:
```json
{
  "path": "op/offset",
  "parameters": {
    "kerf": 5,
    "source": {
      "path": "shape/hexagon",
      "parameters": { "radius": 50 }
    }
  }
}
```
The VFS core automatically normalizes these nested objects to maintain logical identity throughout the pipeline.
