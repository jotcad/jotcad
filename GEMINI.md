# JotCAD Project Context & Memories

## Architecture & Build

Refer to the root [README.md](./README.md) for core architecture and C++ rebuild
instructions. All C++ implementations must link against **`-lcrypto`** and **`-lssl`**.

## Core AI Directives (CRITICAL)

1. **READ BEFORE ACTING:** Before modifying or reasoning about the Virtual File System (VFS), Mesh network, or Geometry Data Models, you MUST read the corresponding specification documents. Do not rely on assumptions.
2. **CONTRACT CHECK:** Before modifying any file listed in the [Documentation Index](#documentation-index), you MUST state the governing protocol rule to the user to confirm alignment.
3. **SECURE CONTEXTS (MOBILE):** WebCrypto API (`crypto.subtle`) is RESTRICTED to secure contexts (HTTPS or localhost). For mobile debugging over a network IP, local HTTPS is MANDATORY.

## Protocol Invariants (TERMINAL RULES)

- **IDENTITY DUALITY (CRITICAL)**: **CID** and **Selector** are top-level alternatives. NEVER wrap a CID in a "fake" Selector. Requests must explicitly signal whether they target a content-hash (CID) or a computational-recipe (Selector).
- **INDEPENDENT MATRIX MANDATE (CRITICAL)**: Shape matrices (`tf`) represent absolute world-space transforms. No scene-graph multiplication (parent-child accumulation) is allowed. All transformations MUST be applied recursively to child components.
- **STRICT SELECTOR ENFORCEMENT**: `normalizeSelector` is a non-coercive type guard. It MUST throw a fatal error if passed a raw object. Hydration MUST happen at the boundary (REST/UX).
- **STABLE HASHING**: The CID of a Selector is `hash(Safe-JCB(Selector_JSON))`. NEVER alter the Selector's structure (keys/paths) during hashing. Standardization (normalization) must happen *before* the Selector reaches the VFS.
- **IMMUTABILITY**: Input artifacts are strictly read-only. Transformation results must be written as NEW anonymous geometry CIDs.
- **VERTICAL DEFAULT**: Standard camera/rendering is top-down vertical (`ax=0, ay=0`).
- **DIRECTORY INDEXING (MANDATORY)**: EVERY directory MUST contain a `README.md` file that describes the directory's core responsibility and provides a high-level summary of its sub-directories. Before researching or modifying a new directory, the AI MUST read its `README.md` to minimize blind state reading and prevent regressions.
- **ATOMIC FILE MANDATORY (SINGLE RESPONSIBILITY)**: To minimize regressions and maximize attention precision, every file MUST have a single, atomic domain of responsibility.
  1. **Cognitive Limit**: If a file's purpose cannot be described in a single sentence, or if it exceeds ~300 lines, it MUST be refactored.
  2. **Subdirectory Conversion**: "Fat" files MUST be converted into a subdirectory. The new directory must contain a `README.md` index and finer-grained files for discrete tasks.
  3. **Explicit Dependencies**: Logic must be moved into these discrete units to ensure all dependencies are explicitly visible via `include` or `import` statements, rather than hidden by proximity in a large file.

## Documentation Index

| Implementation File | Governing Specification | Key Concept |
| :--- | :--- | :--- |
| `fs/cpp/vfs_core.cpp` | `docs/VFS_SPECIFICATION.md` | Recursive Fulfillment & Routing |
| `fs/cpp/cid.cpp` | `docs/CORE_DATA_MODELS.md` | JCB & Cryptographic Identity |
| `fs/cpp/selector.h` | `docs/VFS_SPECIFICATION.md` | Universal Addressing |
| `geo/impl/processor.h` | `docs/JOT_LANGUAGE_SPECIFICATION.md` | Port Injection & Typed Execution |
| `geo/ops/stl_op.h` | `docs/DYNAMIC_OPERATIONS.md` | STL Binary Export |
| `geo/ops/*.h` | `docs/DYNAMIC_OPERATIONS.md` | Kernel Logic & Tolerances |
| `docs/TODO_SIMPLIFICATION.md` | `legacy/` | Edge-Collapse & Garland-Heckbert |

## Universal Protocol Rules

- **STABILITY MANDATE (CRITICAL)**: JS unit tests MUST be executed sequentially (`--test-concurrency=1`) to prevent port deadlocks and race conditions in mesh-heavy tests.
- **PROTOCOL INTEGRITY (CRITICAL)**: Every structured address MUST be a formal `Selector` instance. "String-path guessing" or deconstructing Selectors into top-level keys in metadata or network messages is strictly prohibited.
- **BOUNDARY HYDRATION**: All REST handlers, mesh ingress points, and UI entry points MUST explicitly hydrate incoming JSON into formal `Selector` instances using `Selector.fromObject()`.
- **SEMANTIC GEOMETRY TAGGING (MANDATORY)**: All CAD-produced geometry (Meshes, LineSegments) MUST be tagged with `userData.isJot = true`. Bounding box and auto-zoom calculations MUST exclusively filter for this tag to ignore system helpers (grids, labels).
- **HITCHHIKER OVERLAY GRID**: The shared WebGL context features a dynamic, transparent overlay grid. System objects MUST be tagged with `userData.isSystem = true` and utilize high `renderOrder` with `depthTest: false` to ensure persistent visibility as a scale reference.
- **CONTEXT-SAFE IDENTIFICATION**: Use robust string detection (`Object.prototype.toString.call(val) === '[object String]'`) to identify CID strings and bypass normalization, ensuring stability across bundlers (Vite) and test environments (Puppeteer).
- **ATOMIC STATE EVENTS**: The VFS MUST emit entire `Selector` objects in its events, adhering to the atomic addressing model.
- **JOTCAD CANONICAL BINARY (JCB)**: Tag-prefixed binary format used EXCLUSIVELY to generate stable hashes (CIDs).
- **SAFE IDENTITY (Base64-JCB)**: Base64-encoded JCB used as an ASCII-safe carrier string for JSON transport.
- **SELECTOR WIRE FORMAT**: Network requests MUST wrap the identity in a top-level key: `{"selector": {...}}` OR `{"cid": "..."}`.
- **STACK PROTECTION**: Nodes MUST NOT dispatch to neighbors already present in the `stack` property to prevent infinite loops.
- **FORMAL VFS LINKS**: Use `vfs.link(src, tgt)` for semantic aliasing (the "remainer"). Aliases are strictly metadata-driven and MUST store the full target `Selector`.

## Refined Language Semantics

- **ANCHOR PATTERN (at)**: Implements **Sequential Subject Reduction**. It transforms the entire subject into the inverse frame of each anchor, applies the operation, and projects it back. Multiple anchors result in a single shape modified sequentially.
- **INJECTION PATTERN (on)**: Implements **Targeted Search & Replace**. It performs a non-sequential traversal of the subject hierarchy, replacing sub-components matching a target with the results of an operation applied to them.
- **ROBUST RENDERING**: The `Rasterizer` must support both **Segment rendering** (for wireframes/frames) and **CDT (Constrained Delaunay Triangulation)** for non-convex faces to prevent visual fragmentation.
