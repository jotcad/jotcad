# Core Data Models: Selector, CID, Shape, Geometry

This document defines the strict architectural boundaries between addressing, content identity, and spatial data in JotCAD's Mesh VFS. It is CRITICAL that you understand the distinction in how CIDs are computed.

## The Universal Rule
**Everything in the VFS is addressed by a CID (Content Identifier).** There are no "Address Keys" separate from "Content IDs". However, the *source* of the hash used to compute the CID differs fundamentally between Shapes (semantic metadata) and Geometry (raw math).

**Crucial Distinction:** The main difference between a Selector and a CID is that a **Selector is recomposable** (you can modify its parameters or output port to generate a new Selector), while a **CID is not** (it is a fixed, opaque hash). Geometry always uses a CID because it is not recomposable.

---

## 1. CID (Content Identifier)
A **CID** is a strongly-typed identifier representing a cryptographic hash (SHA-256). 
- **Type Safety:** In C++, it MUST be represented as a formal `struct CID { std::string value; }` rather than a raw `std::string`. This prevents accidental mixing with paths or JSON strings.
- **Storage:** It uniquely identifies a file on disk (e.g., `<CID>.data`).
- **Network & VFS:** CIDs can be transmitted safely over the mesh network. VFS operations like `read` and `write` can natively target either a full `Selector` or a direct `CID`.

---

## 2. Geometry: Content-Addressed (Heavy Data)
**Geometry** is the heavy, raw mathematical representation of an object (vertices, faces, exact kernel structures).
- **How it gets a CID:** Geometry is its own address. Its CID is computed directly from its own raw bytes (`CID = hash(Geometry_Bytes)`).
- **Structured Geometry Format (V F P S T):** JotCAD uses a header-prefixed text format for stable persistence:
  - `V <count>`: Exact rational vertices (`x y z`).
  - `F <count>`: Atomic face definitions (`num_loops loop1_len idx...`).
  - `P, S, T`: Point, Segment, and Triangle primitives.
- **Why:** This guarantees perfect deduplication. If two completely different operations (e.g., `Box(10)` and `Rectangle(10, 10)`) produce the exact same mathematical geometry, they will independently hash to the exact same CID, and the heavy data is only stored once.
- **VFS Interaction:** Geometry is written *anonymously* to the VFS. The VFS hashes the geometry bytes, writes them to `<CID>.data`, and returns the `CID`.

---

## 3. Selector: The Computation Request
A **Selector** describes a specific computation and the exact facet of the result requested.
- **Fields:**
  - `path` (string): The operator or resource (e.g., `"jot/Box"`).
  - `parameters` (map): The input arguments (e.g., `{"width": 10}`).
  - `output` (string, optional): The specific artifact requested.

- **Strict Structural Normalization:** To ensure deterministic CIDs across environments (C++, JS, Node), Selectors MUST be normalized before hashing:
  1. `parameters` MUST always be present (default to `{}`).
  2. `output` MUST be omitted if empty.
  3. Object keys MUST be sorted alphabetically during binary encoding (JCB).

- **Strict read/write Protocol:** VFS methods `read`, `write`, `readData`, and `writeData` strictly require a `Selector` instance or a 64-character hex CID. Standard string paths are prohibited to ensure that every address is a formal, hashable identity.

- **Formal Addressing Mandate:** 
  - **No Implicit Linkage:** A base Selector (without an `output` port) represents the **Operation Identity**, not its result. It is a terminal error for the VFS to automatically resolve a base selector to its `$out` port.
  - **Explicit Port Targeting:** Callers (Compilers, Tests, other Operators) MUST explicitly append the target port (e.g., `:$out`) to retrieve computational results.

---

## 4. Shape: Computation-Addressed (Lightweight Metadata)
A **Shape** is a lightweight, JSON-serializable container that carries the semantic meaning of a resolved object. It is NOT its own address.
- **How it gets a CID:** A Shape's CID is computed from its **Selector** (`CID = hash(Selector)`).
- **Why:** When you ask the VFS for `Box(width=10)`, it hashes that Selector to get `CID_A`. It looks directly for `CID_A.data`. If it exists, the computation is already done and the Shape JSON is returned instantly.
- **Fields:**
  - `geometry`: An optional **CID** (strictly a CID, *never* a Selector). It is a key used by the VFS to look up the `.data` (or `.meta`) for the raw mesh/math.
  - `tf`: An **Exact Rational Matrix** (16-coefficient string) representing the spatial transformation.
  - `tags`: A map of semantic metadata (color, names, etc.).
    - **Topological Identity (`type`)**: Every Shape MUST include a `type` tag to define its topological nature:
      - `points`: 0D discrete vertices.
      - `segments`: 1D wireframes, polylines, or paths.
      - `surface`: **2D Planar**. Strictly coplanar faces (optimized for GPS/2D route).
      - `open`: **3D Open Shell**. Non-planar mesh that does not enclose a volume (e.g., sphere-cap).
      - `closed`: **3D Closed Solid**. A manifold 3D volume (e.g., Box, Orb).
      - `plane`: **Infinite**. Mathematical plane (Z=0 in its local coordinate system).
  - `components`: A list of child Shapes for hierarchical assemblies.
- **Exact Transformation Pipeline:** Every transformation is stored as an exact rational mapping. Precision is NEVER lost during serialization or transport between C++ and JavaScript.
- **VFS Interaction:** During execution, an operator fulfills its own ports by writing to its fulfilling Selector with the appropriate output set (e.g., `vfs->write(fulfilling.with_output("$out"), out_shape)`).

---

## 5. Identity-First Binding (Ordinary Arguments)
Arguments in a Selector (like `$in`) MUST be stable identities (**CIDs** or **Selectors**), never raw JSON data (like full `Shape` or `Geometry` objects).
- **Why:** This ensures that the Selector's own Identity remains stable and independent of the size or complexity of its inputs. It also enables efficient mesh-wide deduplication and caching.
- **Holes & Late-Binding:** In Template Mode (e.g., higher-order operations like `.at(anchor, op)`), the compiler allows affiliate slots (marked `$in` or `$out`) to remain unbound. These unprovided arguments are left as `undefined` (holes) in the Selector, signaling to the runtime provider that they must be filled by the current processing context.
- **Ephemeral Instances:** If an operator generates an intermediate or transformed object (like an oriented corner), it MUST **materialize** that object to get a stable CID before using it as an argument for a subsequent operation.

---

## 6. Cycle Protection (The resolutionStack)
To prevent infinite recursion when following **Formal Links** or recursive mesh requests:
- **Stack Mandate:** Every `read` request MUST carry a `stack` (routing hops) and a `resolutionStack` (link identities).
- **Enforcement:** If a node encounters a CID in the `resolutionStack` that matches the current target, it MUST throw a "Link Cycle" exception.

---

## 7. The Role of .meta Files
Because the VFS is a pure key-value store (`<CID>.data`), `.meta` files are NOT used as routing tables or port maps. A `.meta` file only exists alongside a `.data` file to provide out-of-band network information:
1. **State:** Is this CID `AVAILABLE`, `PENDING` (computation in progress on a worker), or a `LINK`?
2. **Redirection (Links):** If `state == "LINK"`, it holds the target `Selector` to redirect to (handling semantic aliasing).
3. **Type Metadata:** (Optional) MIME types for non-Shape artifacts (e.g., `{"mimeType": "image/png"}`).

## Summary of Operations
1. **Generating Geometry:** Operator computes math -> VFS hashes math to get `GeoCID` -> VFS writes `GeoCID.data` -> VFS returns `GeoCID`.
2. **Generating Shape:** Operator builds Shape JSON embedding `GeoCID` -> VFS hashes the requesting Selector to get `ShapeCID` -> VFS writes Shape JSON to `ShapeCID.data`.
3. **Retrieval:** User requests Selector -> VFS hashes Selector to get `ShapeCID` -> VFS reads `ShapeCID.data` to get Shape JSON -> Consumer reads `Shape.geometry` (`GeoCID`) -> VFS reads `GeoCID.data` to get math.
