# JotCAD Virtual File System (VFS) Specification: Decentralized Mesh

The JotCAD VFS is a distributed, content-addressable mesh system designed for collaborative geometry processing across C++, Node.js, and Browser environments. It uses a **Decentralized Peer-to-Peer** architecture where participants coordinate through recursive bread-crumb routing and local content-addressable storage.

## 1. Identity & Addressing

The VFS uses a **Symmetric Identity** model where the request itself serves as the global address.

### 1.1 The Selector (Wire Identity)
The global identity of any artifact is its **Selector**: a combination of a `path` (e.g., `shape/hexagon`) and a `parameters` object (e.g., `{"radius": 50}`).
- **Normalization:** Selectors are deeply and recursively normalized. Parameters are sorted alphabetically by key, and nested objects are themselves normalized.
- **Deterministic Identity:** Every node calculates the identity of a request locally by hashing the normalized selector (SHA-256).

### 1.2 Identity Secrecy
To prevent "Identity Poisoning," CIDs (Content IDs) are NEVER sent across the network. Nodes only exchange Selectors. Each node is the sole authority over its local CID mapping.

## 2. Peer-to-Peer Protocol (Routing)

The VFS operates as a **Bread-crumb Mesh**. There is no central Hub or Single Point of Failure.

### 2.1 Recursive Bread-crumb READ (`POST /read`)
When a peer requests data that is not available in its local storage or local providers, it performs a mesh search:
1.  **Stack Creation:** The requester appends its own URL to a `stack` array.
2.  **Parallel Forwarding:** The request is sent to all known neighbors.
3.  **Loop Prevention:** A node receiving a request checks if its own URL is in the `stack`. If so, it returns a 404 to block the loop.
4.  **Auto-Peering:** A node "learns" the address of its immediate sender from the stack and adds it to its local neighbors list, creating a symmetric connection dynamically.
5.  **Synchronous Fulfillment:** The provider writes the data to the HTTP response body. Intermediate nodes pipe this stream back to the requester while simultaneously caching it locally.

### 2.2 Demand-Driven Work
The VFS is **Silent by Default**. No work or network traffic occurs until a `read` is initiated.
- **Provider Registration:** Nodes register handlers for specific path prefixes.
- **Implicit Trigger:** A `read` request for a registered path automatically triggers the local provider if the data is missing from disk.

## 3. Immunity & Lifecycle

### 3.1 Ephemeral Lifecycle (The Clean Slate)
Every VFS node starts with an empty local storage.
- **Initialization Wipe:** On startup (`VFS.init`), the node physically deletes all files in its `.vfs_storage` directory.
- **Immunity:** This prevents "Phantom Paths" and stale metadata from previous sessions from contaminating the current mesh.

### 3.2 Local Sovereignty
A node is the absolute authority over its local disk. It only stores data that it has calculated locally or received as a direct response to a request it initiated or forwarded.

## 4. Environment Robustness

The VFS is **Web Stream Standardized**:
- **Unified Streams:** All data transfer uses the **Web Streams API** (`ReadableStream`).
- **Cross-Environment:** This ensures seamless data flow between C++ (via Node wrapper), Node.js services, and the Browser UX.
