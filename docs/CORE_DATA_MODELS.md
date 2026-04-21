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
- **Why:** This guarantees perfect deduplication. If two completely different operations (e.g., `Box(10)` and `Rectangle(10, 10)`) produce the exact same mathematical geometry, they will independently hash to the exact same CID, and the heavy data is only stored once.
- **VFS Interaction:** Geometry is written *anonymously* to the VFS. The VFS hashes the geometry bytes, writes them to `<CID>.data`, and returns the `CID`.

---

## 3. Selector: The Computation Request
A **Selector** describes a specific computation and the exact facet of the result requested.
- **Fields:**
  - `path` (string): The operator or resource (e.g., `"jot/Box"`).
  - `parameters` (map): The input arguments (e.g., `{"width": 10}`).
  - `output` (string, optional): The specific artifact requested. If omitted, it represents the operation itself, not any specific output. We can use formal `.meta` links to associate the operation with a default value (like `$out`) if desired.
- **Role:** It is the "recipe" for a Shape or artifact.

---

## 4. Shape: Computation-Addressed (Lightweight Metadata)
A **Shape** is a lightweight, JSON-serializable container that carries the semantic meaning of a resolved object. It is NOT its own address.
- **How it gets a CID:** A Shape's CID is computed from its **Selector** (`CID = hash(Selector)`).
- **Why:** When you ask the VFS for `Box(width=10)`, it hashes that Selector to get `CID_A`. It looks directly for `CID_A.data`. If it exists, the computation is already done and the Shape JSON is returned instantly.
- **Fields:**
  - `geometry`: An optional **CID** (strictly a CID, *never* a Selector). It is a key used by the VFS to look up the `.data` (or `.meta`) for the raw mesh/math.
  - `tags`: A map of semantic metadata (color, names, etc.).
  - `components`: A list of child Shapes for hierarchical assemblies.
- **VFS Interaction:** During execution, an operation may write to its fulfilling Selector with various outputs set (e.g., `.with_output("thumb")`). A chained operator downstream will receive the `$out` output as its `$in` selector.

---

## 5. The Role of `.meta` Files
Because the VFS is a pure key-value store (`<CID>.data`), `.meta` files are NOT used as routing tables or port maps. A `.meta` file only exists alongside a `.data` file to provide out-of-band network information:
1. **State:** Is this CID `AVAILABLE`, `PENDING` (computation in progress on a worker), or a `LINK`?
2. **Redirection (Links):** If `state == "LINK"`, it holds the target `Selector` to redirect to (handling semantic aliasing).
3. **Type Metadata:** (Optional) MIME types for non-Shape artifacts (e.g., `{"mimeType": "image/png"}`).

## Summary of Operations
1. **Generating Geometry:** Operator computes math -> VFS hashes math to get `GeoCID` -> VFS writes `GeoCID.data` -> VFS returns `GeoCID`.
2. **Generating Shape:** Operator builds Shape JSON embedding `GeoCID` -> VFS hashes the requesting Selector to get `ShapeCID` -> VFS writes Shape JSON to `ShapeCID.data`.
3. **Retrieval:** User requests Selector -> VFS hashes Selector to get `ShapeCID` -> VFS reads `ShapeCID.data` to get Shape JSON -> Consumer reads `Shape.geometry` (`GeoCID`) -> VFS reads `GeoCID.data` to get math.
