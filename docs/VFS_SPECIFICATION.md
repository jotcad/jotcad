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
`path` (e.g., `jot/Hexagon/full`) and a `parameters` object (e.g.,
`{"diameter": 30}`).

- **Normalization:** Selectors are deeply and recursively normalized. Parameters
  are sorted alphabetically by key, and nested objects are themselves
  normalized.
- **Parametric Standardization (Critical):** All radial/apothem parameters MUST be 
  normalized to **`diameter`** before execution. This ensures deterministic 
  Content-IDs (CIDs) regardless of the original construction method (e.g., 
  `Hexagon(radius=15)` and `Hexagon(diameter=30)` resolve to the same CID).
- **Deterministic Identity:** Every node calculates the identity of a request
  locally by hashing the normalized selector (SHA-256).
- **Atomic Address:** The Selector is treated as an atomic unit. API methods 
  consume the entire object (e.g., `vfs.read(selector)`) rather than decomposed 
  paths and parameters.

### 1.2 Identity Secrecy

To prevent "Identity Poisoning," CIDs (Content IDs) are NEVER sent across the
network. Nodes only exchange Selectors. Each node is the sole authority over its
local CID mapping.

## 2. Actor Fulfillment Protocol

JotCAD operators follow a strict **Actor Model** for mesh fulfillment.

### 2.1 The Fulfillment Contract

Every operator call is assigned a unique address (the `fulfilling` Selector). 
The operator's sole responsibility is to satisfy this address by writing its 
final result directly to that Selector:
`vfs->write<Shape>(fulfilling, out_shape)`

### 2.2 Immutable Content (CID Protection)

Input artifacts are strictly read-only. Transformative operators (e.g., `cut`, 
`offset`) MUST NOT "update" the CID-addressed geometry of their inputs. Instead:
1.  **Read:** The operator reads the input geometry from its CID.
2.  **Calculate:** The operator performs the transformation.
3.  **Materialize:** The operator writes the NEW geometry to the mesh using an 
    **anonymous write** (e.g., `vfs->write<Geometry>(res_geo)`).
4.  **Reference:** The final result `Shape` points to this new geometry CID.
5.  **Fulfill:** The `Shape` is written to the operator's assigned `fulfilling` 
    address.

### 2.3 Specialization Rules

- **`Geometry` Materialization:** Writing geometry always requires an empty 
  selector. The mesh computes the hash and returns the unique CID. Providing an 
  explicit path to a geometry write is an error.
- **`Shape` Fulfillment:** Writing a Shape (metadata) requires an explicit 
  address. Providing an empty selector to a Shape write is an error.

## 3. Peer-to-Peer Protocol (Routing)

### 3.1 Identity Introduction (`POST /register`)

The VFS decouples transport connections from peer identities.

- **Handshake:** On connection establishment, a node MUST send a 
  `POST /register?peerId={UUID}` to its neighbor.
- **Identity Mapping:** The receiving node records the mapping of `neighborId` 
  to the specific connection context. This establishes a stable topology.

### 3.2 Formal Links (Unambiguous Aliasing)

The VFS supports **Formal Links**, a mechanism for aliasing one Selector to another. 

- **Link Definition:** A Link is a metadata-driven alias (`vfs.link(src, tgt)`).
- **Recursive Resolution:** Resolution is STRICTLY metadata-driven. The VFS 
  NEVER "guesses" or sniffs links from raw data content.

### 3.3 Recursive Bread-crumb READ (`POST /read`)

The `read` operation is the primary mechanism for demand-driven data retrieval.

- **Unambiguous Signaling:** A `read` MUST either return the requested data 
  with a `200 OK` or fail with an explicit HTTP error code. 

## 4. Discovery Protocol (Catalog)

The VFS uses a **Demand-Driven Push** model for discovery.

### 4.1 Catalog Subscription (SUB)

Nodes discover capabilities by subscribing to **`sys/schema`** or **`sys/topo`**.

- **Immediate Push:** A `SUB` on these system paths triggers an immediate 
  **`CATALOG_ANNOUNCEMENT`** from the neighbor.

### 4.2 Flattened Schema Model

Schemas are self-describing and strictly flattened.

- **Port Mapping:** Logical "Tees" are marked at the port level:
  ```json
  "outputs": {
    "$out": { "type": "shape", "alias": "$in" }
  }
  ```

## 5. Core Type System

### 5.1 Geometry Type (`geometry`)

- **Nature:** Raw spatial payload (text/JOT).
- **Exact Kernel:** All C++ math MUST use the **CGAL Exact Kernel (`FT`)**. 
- **Linkage:** C++ implementations MUST link against **`-lcrypto`** for SHA-256 
  support.

### 5.2 Shape Type (`shape`)

- **Nature:** Semantic container (JSON).
- **Structure:**
  - `geometry`: An optional VFS Selector pointing to a `geometry` artifact.
  - `tf`: 4x4 Affine transformation matrix (Column-Major).
  - `tags`: Key-value metadata.
  - `components`: Nested `shape` objects.
