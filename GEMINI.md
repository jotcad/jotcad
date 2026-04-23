# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`**.

## Core AI Directives (CRITICAL)

1. **READ BEFORE ACTING:** Before modifying or reasoning about the Virtual File System (VFS), Mesh network, or Geometry Data Models, you MUST read the corresponding specification documents. Do not rely on assumptions or past session memory.
2. **Data Models & Addressing:** READ `docs/CORE_DATA_MODELS.md` to understand how Selectors, CIDs, Shapes, and Geometry interact.
3. **VFS & Mesh Architecture:** READ `docs/VFS_SPECIFICATION.md` for routing and execution logic.
4. **Language & Compiler:** READ `docs/JOT_LANGUAGE_SPECIFICATION.md`.

## Universal Protocol Rules

- **PROTOCOL INTEGRITY (CRITICAL)**: Do NOT modify the core VFS Mesh protocols 
  (`read`, `listen`, `register`, `notify`, `subscribe`, `spy`) without explicit 
  discussion. The protocol is a pure, minimal, push/subscribe mesh.
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
- **ATOMIC SELECTOR WIRE FORMAT**: Network requests (`POST /read`, `POST /listen`) MUST wrap the Selector object in a top-level `selector` key. This ensures the protocol remains extensible and uniform across languages.
