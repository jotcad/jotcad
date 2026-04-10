# Geometry Agent Architecture

`@jotcad/geo` is a collection of high-performance geometry processing agents (primarily in C++) that communicate via the `@jotcad/fs` distributed blackboard.

## Core Representations

Following the established patterns in the `geometry/` package, we distinguish between raw **Geometry** and hierarchical **Shapes**.

### 1. Geometry (`/geo/`)
Raw geometric data is stored as human-readable text. This format represents points, edges, and surfaces in a stable, content-addressable way.

#### Text Format (.jot compatible)
- `v <x> <y> <z> <x> <y> <z>`: A vertex. Represented as a pair of min/max coordinates (identically for exact points).
- `p <idx1> <idx2> ...`: A list of point indices.
- `s <idx1> <idx2> <idx3> <idx4> ...`: A list of segment pairs (edges).
- `t <idx1> <idx2> <idx3>`: A triangle.
- `f <idx1> <idx2> ...`: A face boundary (indices of vertices).
- `h <idx1> <idx2> ...`: A hole in the preceding face.

#### VFS Mapping
- **Path:** `geo/mesh`
- **Parameters:** Hash of the content.
- **Properties:** Immutable and content-addressed.

### 2. Shape (`/shape/`)
A Shape is a lightweight JSON object that organizes and transforms geometry.

#### JSON Structure
```json
{
  "geometry": "vfs:/geo/mesh/abc123...",
  "tf": "s 10 10 10",
  "tags": { "color": "red" },
  "shapes": [ ... ],
  "mask": "vfs:/shape/another_shape"
}
```
- `geometry`: URI to a raw Geometry artifact.
- `tf`: Transformation string (e.g., `s` for scale, `t` for translate, `x`/`y`/`z` for rotation).
- `tags`: Key-value metadata.
- `shapes`: Array of child shapes (for groups/unions).
- `mask`: Optional reference to a shape acting as a clipping mask.

## Service Architecture

Agents in this package follow the **Persistent Service** pattern, consolidated into a single `ops` binary:
1. **Startup:** The `ops` binary starts and registers its supported operations (e.g., `shape/box`, `op/offset`) in an internal `Processor::registry()`.
2. **Initialization:** The service iterates through its registered operations, declaring their JSON schemas, uploading Web Component UX definitions to the VFS, and announcing that it is `LISTENING` for requests on their respective paths.
3. **Execution:** 
   - A client requests a shape or operation coordinate (e.g., `shape/box?width=10`).
   - The VFS Hub routes the `READ` command to the `ops` peer's virtual mailbox.
   - The `ops` binary internally dispatches the request to the correct handler (e.g., `box_op`).
   - The handler performs the geometric computation (using CGAL or raw math).
   - The handler writes the resulting raw Geometry (`.jot`) back to the VFS and returns the resulting Shape JSON to fulfill the `READ` request.
4. **Chaining:** Operations like `op/offset` natively support chaining by reading source geometries via VFS URIs (e.g., `vfs:/shape/hexagon`), which automatically triggers cascading evaluations through the VFS Hub.

## Implementation Roadmap

- [x] **Geometry Parser (C++):** Reusable utility to read/write the `.jot` text format.
- [x] **Comprehensive Ops Service:** A single, persistent C++ binary handling multiple operations.
- [x] **Primitive Agents:** Functional evaluators producing `box`, `triangle`, and `hexagon` shapes.
- [x] **Operator Agents:** Transformation agents for `offset` and `outline`.
- [x] **Autonomous Reification:** Operations self-register, declare schemas, and seed UX components.
- [ ] **Boolean Agent:** A heavy-lifting agent for Union, Intersection, and Difference.
- [ ] **Resolution Agent:** Converts "Lazy Shapes" (with transforms) into "Absolute Geometry" (baked vertices).
