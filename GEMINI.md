# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`**.

## Core AI Directives (CRITICAL)

1. **READ BEFORE ACTING:** Before modifying or reasoning about the Virtual File System (VFS), Mesh network, or Geometry Data Models, you MUST read the corresponding specification documents. Do not rely on assumptions.
2. **CONTRACT CHECK:** Before modifying any file listed in the [Documentation Index](#documentation-index), you MUST state the governing protocol rule to the user to confirm alignment.

## Protocol Invariants (TERMINAL RULES)

- **IDENTITY DUALITY (CRITICAL)**: **CID** and **Selector** are top-level alternatives. NEVER wrap a CID in a "fake" Selector. Requests must explicitly signal whether they target a content-hash (CID) or a computational-recipe (Selector).
- **STABLE HASHING**: The CID of a Selector is `hash(Safe-JCB(Selector_JSON))`. NEVER alter the Selector's structure (keys/paths) during hashing. Standardization (normalization) must happen *before* the Selector reaches the VFS.
- **IMMUTABILITY**: Input artifacts are strictly read-only. Transformation results must be written as NEW anonymous geometry CIDs.
- **VERTICAL DEFAULT**: Standard camera/rendering is top-down vertical (`ax=0, ay=0`).

## Documentation Index

| Implementation File | Governing Specification | Key Concept |
| :--- | :--- | :--- |
| `fs/cpp/vfs_core.cpp` | `docs/VFS_SPECIFICATION.md` | Recursive Fulfillment & Routing |
| `fs/cpp/cid.cpp` | `docs/CORE_DATA_MODELS.md` | JCB & Cryptographic Identity |
| `fs/cpp/selector.h` | `docs/VFS_SPECIFICATION.md` | Universal Addressing |
| `geo/impl/processor.h` | `docs/JOT_LANGUAGE_SPECIFICATION.md` | Port Injection & Typed Execution |
| `geo/ops/*.h` | `docs/DYNAMIC_OPERATIONS.md` | Kernel Logic & Tolerances |

## Universal Protocol Rules

- **PROTOCOL INTEGRITY**: Do NOT modify core VFS Mesh protocols (`read`, `listen`, `register`, `notify`, `subscribe`, `spy`) without explicit discussion.
- **JOTCAD CANONICAL BINARY (JCB)**: Tag-prefixed binary format used EXCLUSIVELY to generate stable hashes (CIDs).
- **SAFE IDENTITY (Base64-JCB)**: Base64-encoded JCB used as an ASCII-safe carrier string for JSON transport.
- **SELECTOR WIRE FORMAT**: Network requests MUST wrap the identity in a top-level key: `{"selector": {...}}` OR `{"cid": "..."}`.
- **STACK PROTECTION**: Nodes MUST NOT dispatch to neighbors already present in the `stack` property to prevent infinite loops.
