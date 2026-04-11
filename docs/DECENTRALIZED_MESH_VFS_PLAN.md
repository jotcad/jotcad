# Decentralized Mesh-VFS: Implementation Record

## 1. Summary
The JotCAD VFS has been transformed from a Centralized Hub architecture to a **Decentralized Mesh-VFS**. This transition eliminates the Hub as a Single Point of Failure (SPOF) and resolves systemic metadata corruption ("Phantom Paths") by enforcing local sovereignty and deterministic identity.

## 2. Core Principles

### 2.1. Local Sovereignty & Ephemeral Lifecycle
- **Isolated Storage:** Every node maintains its own independent `.vfs_storage` directory. No state is shared via common disk or database.
- **Clean Slate (Immunity):** On startup, every node physically wipes its local storage (`VFS.init`). This ensures the node is immune to corruption or partial writes from previous sessions.
- **Local-First Writes:** `vfs.write` commits data immediately and only to the node's local disk.

### 2.2. Deterministic Identity (Selector-as-Address)
- **Local CID Calculation:** Every node calculates the CID (SHA-256 hash) of a request locally from its **Selector** (Path + Parameters).
- **Silent Identity:** CIDs are NEVER sent across the network. Nodes only exchange Selectors. This prevents a node from "trusting" a potentially corrupted identity mapping from a neighbor.

### 2.3. Demand-Driven Workflow (Silent Mesh)
- **No Proactive Gossip:** The mesh remains silent until a `READ` is initiated. There are no background state syncs or "Available" broadcasts.
- **Implicit Trigger:** Requesting data is the sole trigger for work. If a provider doesn't have the result, it starts the calculation and holds the connection open until completion.

## 3. Protocol: Symmetric Bread-crumb Routing

### 3.1. The Synchronous `READ` (`POST /read`)
Requests flow through the mesh using a synchronous bread-crumb stack:
1.  **Requester (Node A):** Sends `POST /read` with a `selector` and a `stack` containing its own URL.
2.  **Intermediate (Node B):** Receives the request. 
    - **Loop Prevention:** Checks if its own URL is in the `stack`; if so, it returns 404.
    - **Auto-Peering:** Learns the requester's URL from the stack and adds it to its local neighbors list.
    - **Parallel Search:** Forwards the request to all neighbors NOT in the stack.
3.  **Leaf (Node C):** Finds the data locally, writes it to the response body, and closes the stream.
4.  **Fulfillment:** The bytes flow back through the established bread-crumb chain (C -> B -> A). Intermediate nodes cache the data locally as it passes through.

### 3.2. Technical Standards
- **Web Stream Unification:** All data transfer is standardized on the Web Streams API (`ReadableStream`) to ensure compatibility between Browser and Node.js environments.
- **Parallel Race:** Nodes use `Promise.any` to return the first successful response from neighbors, while allowing other branches to complete and "warm" their local caches.

## 4. Decommissioned Legacy Components
- **The Hub:** `fs/src/vfs_rest_server.js` (Hub logic) has been replaced by the Peer Router.
- **Global SSE:** Proactive SSE-based state synchronization has been removed in favor of synchronous HTTP request-response.
- **RESTBridge:** Replaced by `MeshLink`, which handles multi-neighbor recursive routing and dynamic topology building.

## 5. Deployment Topology
The system is managed by `orchestrator.js` using a tiered peering structure:
- **Dispatcher (9091):** Acts as a leaf provider for native geometry operations.
- **Export (9092):** Connected to Dispatcher; serves as the primary gateway for the UX.
- **UX (Browser):** Connects to the Export Peer; functions as an Originator node.
