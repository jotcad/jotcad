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
    static Shape execute(const Shape& in, double distance, double segments) {
        Shape out = in;
        // ... geometry logic ...
        return out;
    }

    // 3. Schema Definition (Triple-Block)
    static nlohmann::json schema() {
        return {
            {"arguments", {
                {"distance", {{"type", "number"}, {"default", 1.0}}},
                {"segments", {{"type", "number"}, {"default", 16}}}
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
processor.register_op<MyOp, double, double>(vfs, "op/my_op");
```

### 1.2 Triple-Block Schema Mandate

Every operator **MUST** define three blocks in its `schema()`:

1.  **`arguments`**: Literal values or VFS ports (e.g., `number`, `string`, `selector`). Every key here must have a corresponding entry in `argument_keys()`.
2.  **`inputs`**: Formal metadata about required input Shapes (usually labeled `source`).
3.  **`outputs`**: Formal metadata about produced output Shapes (usually labeled `result`).

### 1.3 Vectorized Execution

The `Processor` handles the **Universal Sequence Principle** automatically:
- If a user provides a single value (e.g., `distance: 10`), `execute` is called once.
- If a user provides a sequence (e.g., `distance: [10, 20]`), `execute` is called for each value, and the results are harvested into a unified VFS sequence.
- **Atomic Types:** Use `std::vector<double>` in the `execute` signature if the operator needs to consume a whole sequence at once (e.g., `PointsOp`).

### 1.4 The "Tee" Pattern (Side-Effects)

Some operators, like `jot/pdf` or `jot/stl`, produce side-effects (artifacts) while acting as identity pass-throughs for geometry. These follow the **Output Field Artifact Pattern**:

1.  **Identity Preservation:** The `execute` function performs a pure pass-through (`out = in`).
2.  **On-Demand Generation:** Instead of generating the artifact on every geometry read, the operator's registration logic handles a specific `output` field in the Selector.
3.  **Port Access:** The artifact (e.g., PDF bytes) is only generated and returned when the VFS requests the selector with a specific output (e.g., `output: "file"`).
4.  **Metadata Signaling:** Set `passthrough: true` in the schema metadata to inform the compiler to "lift" the demand into the side-effect list.

```cpp
// Schema defines available outputs
"outputs": {
  "$out": { "type": "shape" },
  "file": { "type": "file", "mimeType": "application/pdf" }
}
```

### 1.5 Two-Stage Resolution (Lazy Execution)

For computationally expensive operations (e.g., complex Booleans or heavy mesh processing), JOT supports a **Two-Stage Resolution** pattern to defer math until it is strictly necessary.

1.  **Stage 1: The Logical Operator**: Returns a `Shape` JSON (the "Passport"). This shape contains metadata (tags, transforms) and formal `.meta` links, but might defer heavy geometry computation.
2.  **Stage 2: The Geometry Provider**: The actual math engine. It is only triggered when a downstream consumer (like a 3D renderer or PDF exporter) strictly demands the terminal geometry CID.

### 1.6 Recursive Reification

To ensure operators always work with valid data, the `Processor` perform **Recursive Reification** on all `Shape` inputs.

- **Automatic Link Following**: If an operator argument is a `Selector` (mesh address), the `Processor` eagerly follows the link to fetch the terminal `Shape` metadata before the operator executes.
- **Unified Consistency**: This reification applies to both single `Shape` arguments and elements within `std::vector<Shape>`, ensuring that nested tools in boolean operations are fully hydrated.

### 1.7 Optional Geometry

A `Shape` in JOT is a recursive container. The `geometry` field is an **optional CID string**.

- **Geometric Nodes**: Shapes that carry a mesh (e.g., a Box) have a `geometry` CID.
- **Structural Nodes**: Shapes that act as containers (e.g., a Group) have `std::nullopt` for geometry but contain `components`.
- **Hybrid Nodes**: Shapes can carry both a local geometry CID and nested components.

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

## 5. HTTPS & Mobile Debugging

To support the **WebCrypto API** (required for CID generation and mesh handshakes) on mobile devices, the development environment must run over **HTTPS**.

### 5.1 Certificate Generation

Use the provided script to generate self-signed certificates for your local network IP:
```bash
# Generates .ssl/localhost-key.pem and .ssl/localhost-cert.pem
./scripts/generate_dev_cert.sh
```

### 5.2 Secure Context Requirement

Browsers (Chrome, Safari) disable `crypto.subtle` on insecure HTTP IPs.
- **Localhost:** Always counts as a Secure Context (HTTP is fine).
- **Network IPs:** REQUIRES HTTPS.
- **Handshake Rule:** When using self-signed certs, you MUST manually visit BOTH the UX port (3030) and the VFS port (9092) in your mobile browser and click "Advanced -> Proceed" for each.

### 5.3 C++ SSL Support

The C++ nodes (`geo/bin/ops`) must be compiled with OpenSSL support to communicate with HTTPS neighbors.
- **Header:** `#define CPPHTTPLIB_OPENSSL_SUPPORT` must be set before including `httplib.h`.
- **Linking:** Link against `-lcrypto` and `-lssl`.
- **Verification:** For development, certificate verification is disabled in the C++ core to accommodate self-signed local certs.
