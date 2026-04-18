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

Some operators, like `jot/pdf` or `jot/stl`, produce side-effects (artifacts) while acting as identity pass-throughs for geometry. These follow the **Port-Based Artifact Pattern**:

1.  **Identity Preservation:** The `execute` function performs a pure pass-through (`out = in`).
2.  **On-Demand Generation:** Instead of generating the artifact on every geometry read, the operator's registration logic handles a `$port` parameter.
3.  **Port Access:** The artifact (e.g., PDF bytes) is only generated and returned when the VFS requests the selector with a specific port (e.g., `parameters: { "$port": "filename" }`).
4.  **Metadata Signaling:** Set `passthrough: true` in the schema metadata to inform the compiler to "lift" the demand into the side-effect list.

```cpp
// Schema defines the Port
"outputs": {
  "$out": { "type": "shape" },
  "filename": { "type": "file", "mimeType": "application/pdf" }
}
```

### 1.5 Two-Stage Resolution (Lazy Geometry)

For computationally expensive operations (e.g., complex Booleans or heavy mesh processing), JOT supports a **Two-Stage Resolution** pattern to defer math until it is strictly necessary.

1.  **Stage 1: The Logical Operator**: Returns a `Shape` JSON (the "Passport"). This shape contains metadata (tags, transforms) but its `geometry.path` points to a **Geometry Provider** instead of raw data. This stage is extremely fast and CID-stable.
2.  **Stage 2: The Geometry Provider**: The actual math engine. It is only triggered when a downstream consumer (like a 3D renderer or PDF exporter) calls `vfs.read()` on the geometry selector found in the Passport.

Use this pattern when the cost of calculating vertices exceeds the overhead of an additional mesh request ($>100ms$).


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

## 4. Build System

JotCAD uses a tiered **Makefile** build system for high-performance incremental compilation.

### 4.1 Core Library
The VFS core and all operator logic are compiled into a static library: `geo/bin/libjotgeo.a`. This allows both the production binary and unit tests to share the exact same compiled logic.

### 4.2 Mandatory Testing
Unit tests are **integrated into the build process**. A build is only considered successful if all operator tests pass.

```bash
# Build everything and run all unit tests
cd geo && ./build.sh

# Run tests independently
npm run test:unit
```

### 4.3 Clean Builds
To perform a full clean and rebuild:
```bash
cd geo && make clean && make
```
