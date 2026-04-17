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

### 1.4 The "Tee" Pattern & Side-Effects

Some operators, like `jot/pdf` or `jot/stl`, are designed to produce side-effects (files in the VFS) without altering the geometric result. These follow the **Tee Pattern**:

1.  **Identity Preservation:** The `execute` function should perform a pure pass-through (`out = in`).
2.  **VFS Sink:** Use `vfs->write()` to persist the side-effect (e.g., PDF bytes) at a specific path.
3.  **Metadata Signaling:** Set `optimizeAliases: true` in the schema metadata to inform the compiler that this operation does not change the Content-ID of the shape.

```cpp
static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& filename, Shape& out) {
    // 1. Generate side-effect data
    auto data = generate_my_format(in);
    
    // 2. Sink to VFS
    vfs->write(filename, {}, data);

    // 3. Pure Pass-through
    out = in;
}
```

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

The C++ node is built using `geo/build.sh`. It produces a single static binary `geo/bin/ops` which hosts the entire operator catalog.

```bash
./geo/build.sh
```
