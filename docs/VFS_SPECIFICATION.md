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
- `path` (e.g., `jot/Hexagon/full`)
- `parameters` (e.g., `{"diameter": 30}`)
- `output` (e.g., `"thumb"` or `"$out"`. If omitted, it targets the operation itself).

- **Atomic Address:** The Selector is treated as an atomic unit. API methods consume the entire object (e.g., `vfs.read(selector)`). Hashing the *entire* Selector (including the `output` field) yields the CID for that specific artifact.
- **Parametric Standardization:** Parameters MUST be normalized (e.g., radial/apothem parameters to `diameter`) before execution to ensure deterministic CIDs.

### 1.2 Network Transmission

CIDs CAN and SHOULD be transmitted safely over the mesh network. Nodes can request data directly by CID, or they can request the execution of a computation by sending a Selector.

## 2. Actor Fulfillment Protocol

JotCAD operators follow a strict **Actor Model** for mesh fulfillment.

### 2.1 The Fulfillment Contract

Every operator call is assigned a unique address (the `fulfilling` Selector). 
The operator's sole responsibility is to satisfy this address by writing its final result. A single execution may fulfill multiple outputs by modifying the Selector (e.g., `fulfilling.with_output("thumb")`) and writing the respective artifacts.

### 2.2 Immutable Content (CID Protection)

Input artifacts are strictly read-only. Transformative operators (e.g., `cut`, `offset`) MUST NOT "update" the CID-addressed geometry of their inputs. Instead:
1.  **Read:** The operator reads the input geometry from its CID.
2.  **Calculate:** The operator performs the transformation.
3.  **Materialize:** The operator writes the NEW geometry to the mesh using an **anonymous write** (`vfs->write(res_geo)`), which returns a new CID.
4.  **Reference:** The final result `Shape` embeds this new geometry CID.
5.  **Fulfill:** The `Shape` is written to the operator's assigned `fulfilling` address.

### 2.3 Flat Port Architecture

There are NO "Partitioned Output Ports" or `ports` maps in `.meta` files.
Every distinct output port (e.g., `$out`, `file`, `thumb`) is just a field in the Selector. Because the `output` field is hashed along with the path and parameters, every artifact gets its own unique, deterministic CID and its own discrete `.data` file.

## 3. Peer-to-Peer Protocol (Routing)

### 3.1 Identity Introduction (`POST /register`)

The VFS decouples transport connections from peer identities. On connection establishment, a node MUST send a `POST /register?peerId={UUID}` to its neighbor.

### 3.2 Formal Links (Unambiguous Aliasing)

The VFS supports **Formal Links**, a mechanism for aliasing one Selector to another. 
- **Link Definition:** A Link is a metadata-driven alias (`vfs.link(src, tgt)`).
- **Storage:** The VFS writes a `.meta` file at the source CID containing `{"state": "LINK", "target": tgt_selector}`.

### 3.3 Recursive Bread-crumb READ (`POST /read`)

The `read` operation is the primary mechanism for demand-driven data retrieval.

- **Atomic Wire Format:** Requests MUST wrap the target Selector or CID in a top-level key.
  ```json
  {
    "selector": { "path": "jot/Box", "parameters": { "width": 10 }, "output": "file" },
    "expiresAt": 1700000000000,
    "stack": ["node-A"]
  }
  ```
  Or, for direct CID retrieval:
  ```json
  {
    "cid": "deadbeef1234567890...",
    "expiresAt": 1700000000000,
    "stack": ["node-A"]
  }
  ```

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
  - `tf`: 4x4 Affine transformation matrix (Column-Major).
  - `tags`: Key-value metadata.
  - `components`: Nested `shape` objects.
