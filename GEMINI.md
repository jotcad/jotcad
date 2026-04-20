# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`**.

## AI Operational Mandates

- **PROTOCOL INTEGRITY (CRITICAL)**: Do NOT modify the core VFS Mesh protocols 
  (`read`, `listen`, `register`, `notify`, `subscribe`, `spy`) without explicit 
  discussion. The protocol is a pure, minimal, push/subscribe mesh. 
- **ADDRESS vs CONTENT**: Strictly separate the mesh address from the content.
  - **Selector Key**: `hash(normalize(Selector))` identifies the *address* (the `.meta` pointer).
  - **CID**: `hash(Safe-JCB)` identifies the *content* (the terminal `.data`).
- **SAFE IDENTITY (Base64-JCB)**: Structured data is stored as ASCII-safe strings.
  - **The Flow**: `json` -> **JCB (Binary)** -> **Base64 (Safe String)** -> **Hash (CID)** -> **Disk**.
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
- **SIDE-WRITE PATTERN**: Operators producing auxiliary artifacts (e.g., `jot/pdf`) SHOULD fulfill the primary address with the transformed shape (pass-through) and write the secondary artifact to a derived address by adding a specific parameter (e.g., `$path`) to the fulfilling selector. This keeps artifacts introspectable and avoids "Tee" patterns that hide data.
