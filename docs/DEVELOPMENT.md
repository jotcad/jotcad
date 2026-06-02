# JotCAD Development Guide

This guide covers the internal architecture and patterns for extending JotCAD, specifically for implementing C++ geometry operators.

## 1. C++ Operator Architecture

JotCAD uses a unified **Processor** to bridge the VFS JSON protocol with high-performance C++ geometry kernels. Operators are registered using a variadic template pattern that automates argument decoding and vector expansion.

### 1.1 The Unified `register_op` Pattern

Every C++ operator must inherit from `Op` (or a specialized base) and be registered in `geo/ops.cc`.

```cpp
// Example: geo/my_op.h
struct MyOp {
    // 1. Define Argument Keys (Must match schema 'arguments' block)
    static std::vector<std::string> argument_keys() { 
        return {"distance", "segments"}; 
    }

    // 2. Vectorized Execution Logic
    // Processor automatically expands sequence inputs (e.g., [10, 20])
    // and calls execute() for each permutation.
    static Shape execute(const Shape& in, Interval distance, double segments) {
        Shape out = in;
        // ... geometry logic ...
        return out;
    }

    // 3. Schema Definition (Triple-Block)
    static nlohmann::json schema() {
        return {
            {"arguments", {
                {"distance", {{"type", "jot:interval"}, {"default", 1.0}}},
                {"segments", {{"type", "jot:number"}, {"default", 16}}}
            }},
            {"inputs", {{"source", {{"type", "geometry"}}}}},
            {"outputs", {{"result", {{"type", "geometry"}}}}},
            {"metadata", {{"alias", "jot/myOp"}}}
        };
    }
};

// 4. Registration in geo/ops.cc
// Processor::register_op<Op, T...>(vfs, "op/path")
// T... must match the trailing arguments of execute()
processor.register_op<MyOp, Interval, double>(vfs, "op/my_op");
```

### 1.2 Triple-Block Schema Mandate

Every operator **MUST** define three blocks in its `schema()`:

1.  **`arguments`**: Literal values or VFS ports (e.g., `jot:number`, `jot:string`, `jot:interval`). Every key here must have a corresponding entry in `argument_keys()`.
2.  **`inputs`**: Formal metadata about required input Shapes (usually labeled `source`).
3.  **`outputs`**: Formal metadata about produced output Shapes (usually labeled `result`).

### 1.3 Vectorized Execution

The `Processor` handles the **Universal Sequence Principle** automatically:
- If a user provides a single value (e.g., `distance: 10`), `execute` is called once.
- If a user provides a sequence (e.g., `distance: [10, 20]`), `execute` is called for each value, and the results are harvested into a unified VFS sequence.
- **Interval Normalization:** Dimensional arguments (like `distance`) should use the `Interval` type. The `Processor` automatically handles conversion from scalars to symmetric intervals.
- **Atomic Types:** Use `std::vector<double>` in the `execute` signature if the operator needs to consume a whole sequence at once (e.g., `PointsOp`).

### 1.5 Geometry Well-Formedness Assertions

All 3D shape generators and boolean operations MUST be verified for topological integrity and positive volume using the `verify_well_formed_solid` helper in `geo/test/test_base.h`.

- **Exact Rational Math (FT)**: Assertions are performed using CGAL's Exact Kernel. Never use `double` for volume or coordinate comparisons.
- **Topological Closure**: Verifies the mesh is manifold and watertight (`CGAL::is_closed`).
- **Orientation**: Verifies the mesh is outward-oriented to ensure positive volume calculations.
- **Positive Volume**: Asserts that the shape bounds a strictly positive volume (`vol > FT(0)`).

```cpp
// Example: Verifying a generated shape
Geometry my_geo = vfs.read<Geometry>(my_selector);
vfs.verify_well_formed_solid(my_geo, "My Operation Label");
```

### 1.5 Two-Stage Resolution (Lazy Execution)

For computationally expensive operations (e.g., complex Booleans or heavy mesh processing), JOT supports a **Two-Stage Resolution** pattern to defer math until it is strictly necessary.

1.  **Stage 1: The Logical Operator**: Returns a `Shape` JSON (the "Passport"). This shape contains metadata (tags, transforms) and formal `.meta` links, but might defer heavy geometry computation.
2.  **Stage 2: The Geometry Provider**: The actual math engine. It is only triggered when a downstream consumer (like a 3D renderer or PDF exporter) strictly demands the terminal geometry CID.

### 1.6 Recursive Reification

To ensure operators always work with valid data, the `Processor` perform **Recursive Reification** on all `Shape` inputs.

- **Automatic Link Following**: If an operator argument is a `Selector` (mesh address), the `Processor` eagerly follows the link to fetch the terminal `Shape` metadata before the operator executes.
- **Unified Consistency**: This reification applies to both single `Shape` arguments and elements within `std::vector<Shape>`, ensuring that nested tools in boolean operations are fully hydrated.

### 1.7 Optional Geometry & Absolute Transforms

A `Shape` in JOT is a recursive container. The `geometry` field is an **optional CID string**.

- **Geometric Nodes**: Shapes that carry a mesh (e.g., a Box) have a `geometry` CID.
- **Structural Nodes**: Shapes that act as containers (e.g., a Group) have `std::nullopt` for geometry but contain `components`.
- **Hybrid Nodes**: Shapes can carry both a local geometry CID and nested components.
- **Absolute Identity Mandate (CRITICAL)**: Every `Shape` in a hierarchy carries its own **Absolute World-Space Transform (`tf`)**. Parents MUST NOT apply their transform to their children during construction, and Renderers MUST NOT multiply parent and child transforms. This ensures that every sub-component remains semantically extractable and spatially correct regardless of its position in a group.

Operators MUST check `shape.geometry.has_value()` before attempting to read geometry bits from the VFS.

### 1.8 Hierarchical Mapping (e.g., `cut`, `offset`)

Advanced operators MUST follow the **Hierarchical Mapping** principle to preserve part integrity:
- **No Implicit Union**: Operators should never automatically merge a subject's geometry with its children.
- **Recursive Walk**: The operation should be applied independently to every geometry node in the shape tree.
- **Structure Preservation**: The output of an operation on a `Group` should still be a `Group`, with the operation mapped across its components.

### 1.9 Conjugation & The Workbench (`on`)

The `jot/on` operator implements the **Conjugation Pattern**: $T \times op \times T^{-1}$.

1.  **Clamping ($T^{-1}$)**: The subject is moved from its global position to the workbench origin $(0,0,0)$.
2.  **Operation ($op$)**: The specified operation (recipe) is executed in the local workbench space.
3.  **Unclamping ($T$)**: The result is moved back to the global position.

When given a group of target frames, `jot/on` acts as an **Accumulator (Fold)**, sequentially applying the operation to each frame using the result of the previous step. This ensures features like multiple corner cuts correctly accumulate on a single manifold part.


## 2. Naming & Casing Standards

- **Constructors (PascalCase):** `jot/Box`, `jot/Hexagon`.
- **Operations (camelCase):** `jot/offset`, `jot/rotateX`.
- **Namespacing:** All public operators MUST use the `jot/` prefix in their `metadata.alias`.

## 3. VFS Exception Handling

Operators should throw `VFSException` (defined in `geo/impl/vfs_node.h`) for unambiguous error signaling across the mesh:

- `404`: Content not found.
- `400`: Invalid parameters.
- `508`: Loop detected (handled by MeshLink).
- `429`: Rate limited/Resource exhausted.

## 6. Test Orchestration & Stability

JotCAD uses a unified **Test Orchestrator** to manage the execution of multiple test suites across different languages (JS, C++) and environments (Node.js, Browser/Puppeteer).

### 6.1 Unified Test Entry Point

The primary way to run tests is via the top-level `npm` scripts:

```bash
# Run all verified stable tests (JOT, GEO, FS, Integration)
npm test

# Run all tests, including Puppeteer browser-side integration
npm run tests
```

Both scripts utilize `test_orchestrator.js`, which ensures that each suite is run in a clean environment.

### 6.2 Resource Cleanup & Port Isolation

To prevent network port collisions (`EADDRINUSE`) and file system locks, the orchestrator implements strict **Port Isolation**:

1.  **Disjoint Port Mapping**: Profiles use unique port ranges to prevent back-to-back test failures.
    *   **test/standard**: Ops on `9191`, Export on `9197`.
    *   **live/direct_cpp**: Ops on `9198` (Reserved for direct UX bridge).
2.  **Storage Purge**: Temporary VFS storage directories (prefixed with `.vfs_storage_test_`) are deleted between suite runs.
3.  **Graceful Teardown**: The `sys.stop()` method ensures child processes are killed and network sockets are fully released by the OS before the next test begins.

### 6.3 Dynamic Gateway Discovery

Browser-side integration tests are isolated in `integration/puppeteer/`. To support disjoint ports without multiple builds, the UX implements **Dynamic Gateway Discovery**:

1.  **URL Override**: The UX checks for a `?gateway=` query parameter. If present, it overrides the build-time `VITE_VFS_URL`.
2.  **Orchestrator Injection**: When `launchSystem` is called, it returns a `gatewayUrl` (e.g., `https://localhost:3131/?gateway=9197`).
3.  **Test Execution**: Puppeteer tests MUST use `page.goto(cluster.gatewayUrl)` to ensure the browser connects to the correct cluster.

### 6.5 Secure Contexts & HTTPS

JotCAD utilizes the WebCrypto API (`crypto.subtle`) and other browser features that are restricted to **Secure Contexts**.

1.  **Mandatory HTTPS for Mesh Communication**: To ensure cross-origin requests between the UX and the Export/Ops nodes are permitted, all nodes in the `TEST` and `LIVE` profiles must run over HTTPS when accessed from a network IP.
2.  **Self-Signed Certificates**: Development uses self-signed certificates. The `Orchestrator` automatically detects the `.ssl/` directory and configures nodes for HTTPS.
3.  **Integration Test Protocol**:
    *   The UX test build (`npm run build:test`) is hardcoded to use `https://localhost:9192`.
    *   The `orchestrator.js` `TEST` profile MUST include the `--ssl` flag for the UX server.
    *   Puppeteer tests MUST be launched with the `--ignore-certificate-errors` flag to permit communication with self-signed native nodes.

## 7. Topological Mandates

### 7.1 Wire-to-Face Transition

Operators in JotCAD have strict topological domains. Specifically, **`jot/offset`** and **`jot/pdf`** operate on **Regions (Faces)**, not **Wires (Segments)**.

- **Mandate**: 1D Segments (produced by `Link` or `Loop`) MUST be explicitly promoted to 2D Faces before being passed to an offset or export engine that requires surface area.
- **Operator**: Use **`jot/fill()`** to perform this promotion.
- **Rationale**: This prevents silent failures where an offset of a line results in empty geometry, and ensures that the user's intent regarding winding rules and parity (holes) is explicitly declared.

```js
// CORRECT: Explicit fill before offset
loop(points).fill().offset(diameter=5)

// INCORRECT: Offset will return empty geometry for segments
loop(points).offset(diameter=5)
```


### 5.1 Certificate Generation

Use the provided script to generate self-signed certificates for your local network IP:
```bash
# Generates .ssl/localhost-key.pem and .ssl/localhost-cert.pem
./scripts/generate_dev_cert.sh
```

### 5.2 Secure Context Requirement

Browsers (Chrome, Safari) disable `crypto.subtle` on insecure HTTP IPs.
- **Localhost:** Always counts as a Secure Context (HTTP is fine), but integration tests use HTTPS to ensure parity.
- **CI Environment:** Certificates are automatically generated in GitHub Actions.
- **Graceful Fallback:** The `orchestrator.js` automatically detects certificates in `.ssl/`. If missing, it falls back to HTTP mode for local development.
- **Network IPs:** REQUIRES HTTPS.
- **Handshake Rule:** When using self-signed certs, you MUST manually visit BOTH the UX port (3030) and the VFS port (9092) in your mobile browser and click "Advanced -> Proceed" for each.

### 5.3 C++ SSL & Library Dependencies

The C++ nodes (`geo/bin/ops`) must be compiled with OpenSSL and FreeType support.
- **Header:** `#define CPPHTTPLIB_OPENSSL_SUPPORT` must be set before including `httplib.h`.
- **Linking:** Link against `-lssl`, `-lcrypto`, `-lz` (for static FreeType), and `-ldl`.
- **Order Mandate:** `-lssl` MUST precede `-lcrypto`.
- **Linker Groups:** Use `-Wl,--start-group` and `-Wl,--end-group` to resolve transitive and circular dependencies between static archives.
- **Verification:** For development, certificate verification is disabled in the C++ core to accommodate self-signed local certs.

### 5.4 Forcing HTTP Mode

In certain integration test scenarios (like Puppeteer), HTTPS can trigger "Mixed Content" blocks or certificate errors. You can force the UX server to run in HTTP mode:
```bash
VITE_HTTPS=false npm run unit_test
```
The `ux/vite.config.js` respects this flag even if local certificates are present.
