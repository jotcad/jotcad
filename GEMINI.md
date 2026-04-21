# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`**.

## AI Operational Mandates

- **PROTOCOL INTEGRITY (CRITICAL)**: Do NOT modify the core VFS Mesh protocols 
  (`read`, `listen`, `register`, `notify`, `subscribe`, `spy`) without explicit 
  discussion. The protocol is a pure, minimal, push/subscribe mesh. 
- **ADDRESS vs CONTENT**: Strictly separate the mesh address from the content.
  - **Selector Key**: `hash(JCB(normalize(Selector)))` identifies the *address* (the `.meta` pointer).
  - **CID**: `hash(JCB(val))` identifies the *content* (the terminal `.data`).
- **JOTCAD CANONICAL BINARY (JCB)**: A tag-prefixed binary format used to produce stable, deterministic identities across JavaScript and C++.
  - **Identity-First**: JCB is used EXCLUSIVELY to generate stable hashes (CIDs). It ensures that logically equivalent objects (regardless of key order) share the same address.
  - **Storage**: The actual `.data` content on disk remains the **raw representation** (e.g., readable JSON or binary blobs). To verify a CID for an object, the VFS must parse the storage content and apply the JCB transform before hashing.
- **SAFE IDENTITY (Base64-JCB)**: A Base64-encoded version of the JCB binary used as an ASCII-safe carrier string.
  - **JSON Safety**: Use Safe-JCB ONLY when a canonical identity (CID or Selector) must be embedded as a single string value within a JSON message (like mesh transport) to avoid parsing discrepancies or escaping issues.
  - **Zero Escaping**: Base64 characters are safe constituents of JSON strings and never require escaping.
- **PROTOCOL REFINEMENTS**:
  - **Strict readData**: `readData(selector)` MUST receive a full Selector object (path + parameters). Passing split arguments is a terminal error.
  - **Stack Protection**: The `stack` property in mesh requests contains Node IDs. Nodes MUST NOT dispatch to neighbors already present in the stack to prevent infinite routing loops.
- **SELECTOR NORMALIZATION**: Simplified to structural standardization only.
  - **Identity via Traversal**: The JCB encoder walks keys in alphabetical order for hashing. The source object remains untouched.
  - **No Semantic Magic**: Do NOT "fix" or transform user parameters (e.g., `radius` to `diameter`) during normalization.
- **FORMAL VFS LINKS**: Use `vfs.link(src, tgt)` to handle semantic aliasing (the "remainer"). Aliases are strictly metadata-driven.
- **ACTOR FULFILLMENT MODEL**: Operators return `void` and MUST satisfy their assigned address by calling `vfs->write<Shape>(fulfilling, out)`.
- **IMMUTABLE GEOMETRY**: Primitives and transformative operators MUST NOT overwrite input CIDs. New geometry must be materialized via anonymous writes: `vfs->write<Geometry>(res)`.
- **ATOMIC SELECTOR WIRE FORMAT**: Network requests (`POST /read`, `POST /listen`) MUST wrap the Selector object in a top-level `selector` key. This ensures the protocol remains extensible and uniform across languages.
- **PARTITIONED OUTPUT PORTS**: A selector defines a singular *computation* (the Base Selector), which produces a cached result mapping. Callers request a specific facet of this result via an `output` port specifier (e.g., `output: '$out'` or `output: 'file'`).
  - **Identity Separation**: Do NOT modify the `parameters` to retrieve different artifacts from the same computation. The Base Selector remains immutable, ensuring canonical hashing.
  - **The Plan**: In the next phase, update the wire format to include an optional `output` field (defaulting to `$out` if omitted). Update `VFSNode::write` and `vfs.write` to store result maps (keyed by port) when fulfilling a computation, and update `readData` to extract the requested port slice. Remove the old `$path` side-write hacks.
