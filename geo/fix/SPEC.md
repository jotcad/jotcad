# Specification: `repair_self_touches` (Manifold Recovery)

**Reference**: See [docs/TOPOLOGICAL_SINGULARITIES.md](../../docs/TOPOLOGICAL_SINGULARITIES.md) for the foundational problem definition and resolution classes.

## 1. Objective
Implement a robust manifold recovery pass in the JotCAD native kernel to resolve **Zero-Volume Contacts** (Topological Singularities). The goal is to ensure that all geometric results are strictly manifold and immune to inadvertent spatial merging.

## 2. API Design

### `is_geometry_unambiguous<K>(mesh)`
- **Predicate**: Returns `true` if the mesh is strictly manifold and free of "pinched" vertices or edges.
- **Usage**: Use this to determine if a geometry requires resolution before downstream operations.

### `make_geometry_unambiguous<K>(mesh, delta)`
- **Action**: Resolves non-manifold singularities using **Umbrella Splitting & Geometric Locking**.
- **Transformation**: Localizes distortion via subdivision and applies coordinate displacement to ensure topological shells have distinct geometric points.

## 2. Core 3D Algorithm: Umbrella Splitting & Geometric Locking
This algorithm targets "bowtie" vertices and "hinge" edges by localizing geometric distortion through subdivision.

### Step 1: Identification
- Use `CGAL::Polygon_mesh_processing::non_manifold_vertices` to identify "pinched" vertices.
- A vertex is non-manifold if its incident faces form more than one disjoint cycle (umbrella).

### Step 2: Combinatorial Splitting
- Use `CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices` with an `output_iterator`.
- **Output**: A set of vertex groups, where each group `[V_orig, V_new1, V_new2, ...]` represents topologically distinct umbrellas sharing a coordinate.

### Step 3: Local Subdivision (Planarity Preservation)
For each vertex `V` in a group:
1.  **Identify Edges**: Collect all outgoing halfedges from `V`.
2.  **Split Edges**: For each halfedge, insert a new vertex `V_prime` at an infinitesimal distance `delta` (default: `0.0001`) from `V`.
    - Use `CGAL::Euler::split_edge(edge, mesh)`.
3.  **Split Faces**: Connect the new `V_prime` vertices by splitting the incident faces using `CGAL::Euler::split_face`.
    - This creates a small "transition triangle" (the cap) that isolates `V` from the rest of the face.

### Step 4: Geometric Perturbation
1.  **Calculate Offset**: Compute the average vector of all `(V_prime - V)` offsets for this specific umbrella.
2.  **Displace Apex**: Move the apex vertex `V` by `average_offset / 2`.
3.  **Result**: Topologically distinct components now have distinct geometric coordinates.

## 3. 2D Algorithm: Bisector-Based Gap Injection
Targets results from `jot/cut` and `jot/join` where separate boundaries "kiss" at a vertex.

### Step 1: Detection
- Iterate through all polygons in the result set (`General_polygon_set_2`).
- Identify vertices from different boundaries (or different loops of the same polygon) that share a coordinate.

### Step 2: Perturbation
- For each coincident vertex $V$:
  1. Calculate the **inward-pointing bisector** of the two incident edges.
  2. Displace $V$ along this bisector by `delta`.
  3. This "shrinks" the polygon locally, creating a visible (but infinitesimal) gap.

## 4. Implementation Plan
1.  **`geo/impl/repair.h`**: Add `repair_self_touches<K>(Surface_mesh& mesh)` function.
2.  **`geo/cut_op.h`**: Integrate the 2D recovery pass into `gps_to_geometry`.
3.  **`geo/test/fix_test.cpp`**: 
    - Test Case A: Two squares sharing a corner (2D).
    - Test Case B: Two cones sharing an apex (3D).
    - Verification: `is_manifold(mesh)` must return `true`.

## 5. Numerical Constraints
- **Exactness**: Identify coincident points using **Exact Predicates** (`EK`).
## 6. Status
- **Implementation**: [COMPLETED] Core logic added in `geo/fix/repair.h`.
- **Verification**: [VERIFIED] All tests in `geo/fix/repair_test.cpp` passed.
- **Integration**: [READY] The pass is available for integration into Boolean and Rule operators.
