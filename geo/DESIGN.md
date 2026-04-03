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

## Evaluator Pattern

Agents in this package follow the **Single-Task Evaluator** pattern:
1. **Trigger:** A dispatcher hears a `PENDING` request for a coordinate (e.g., `shape/box?width=10`).
2. **Instantiate:** The dispatcher spawns the relevant C++ agent process.
3. **Execution:** The agent:
   - Reads input dependencies from the Blackboard.
   - Performs geometric computation (e.g., via CGAL or Manifold).
   - Writes the resulting Geometry and Shape back to the Result Plane.
4. **Dissolve:** The process exits, freeing all resources.

## Implementation Roadmap

- [ ] **Geometry Parser (C++):** A reusable utility to read/write the `.jot` text format.
- [ ] **Box Agent:** The first functional evaluator producing box primitives.
- [ ] **Boolean Agent:** A heavy-lifting agent for Union, Intersection, and Difference.
- [ ] **Resolution Agent:** Converts "Lazy Shapes" (with transforms) into "Absolute Geometry" (baked vertices).
