# Problem Specification: Zero-Volume Contact & Manifold Recovery

## 1. The Challenge: Topological Singularities
In CAD operations (specifically `cut`, `join`, and `rule`), geometric results often produce "zero-volume contacts" where distinct manifold components share a singular vertex or edge. 

While mathematically valid, these configurations are "unstable" because:
- **Spatial Deduplication**: Standard mesh kernels and "repair" algorithms merge coincident vertices, turning two independent manifold shells into a single **non-manifold** entity.
- **Boolean Failure**: Subsequent operations on non-manifold geometry often fail or produce unpredictable results.
- **Numerical Noise**: Near-zero distances at these contact points lead to precision errors in exact predicates.

## 2. Classes of Zero-Volume Contact

### A. Point-to-Point (The Bowtie)
- **2D**: Two polygons sharing a single vertex.
- **3D**: Two polyhedra (e.g., cones) sharing an apex.
- **Topological State**: Non-manifold vertex (the Link is not a single disk/cycle).

### B. Edge-to-Edge (The Hinge)
- **3D**: Two polyhedra sharing an edge but no adjacent faces.
- **Topological State**: Non-manifold edge (3+ faces sharing an edge).

### C. Surface-to-Surface (The Membrane)
- **3D**: Two polyhedra sharing a face with opposite normals (coincident faces).

### D. Point-to-Surface (The Apex/Pierce)
- **3D**: A vertex of one body touching the interior or boundary of a face of another body.
- **Degenerate Rule**: A ruled surface connecting a loop to a single point (creating a cone/pyramid).
- **Topological State**: Non-manifold vertex where the surface "pinches" to a zero-area contact.

## 3. Resolution Strategies

### I. Infinitesimal Separation (The "Split")
To prevent spatial merging, topologically distinct vertices must be moved apart.
- **Local Subdivision**: Before moving the vertex, subdivide the incident edges/faces within a small radius $\delta$ of the singularity.
- **Perturbation**: Displace the original vertex along its local manifold normal or "center of mass" to break the coordinate identity.
- **Benefit**: Preserves $99.9\%$ of the original face planarity.

### II. Non-Zero Volume Expansion (The "Bridge")
Convert the zero-volume contact into a physical manifold connection.
- **Capping**: "Clip" the touching tips and insert a small polygonal bridge (a neck) between them.
- **Benefit**: Extremely robust for physical manufacturing simulations; avoids "magic" zero-width connections.

### III. Recovery & Geometric Locking
Since we assume that **any vertices sharing a coordinate will eventually be merged** (by the kernel, a renderer, or an exporter), the geometry must "defend" its own manifoldness.

1.  **Topological Recovery**: Analyze the input "soup" or operation trace to determine which faces belong to the same manifold shell.
2.  **Geometric Locking**: 
    - For vertices belonging to different shells that share a coordinate, **separate them geometrically**.
    - Use the **Subdivision + Perturbation** strategy to ensure they have distinct points.
    - This "locks" the manifold state into the geometric data, making it immune to coordinate-based merging.

## 4. Scope of Implementation
- **2D (GPS)**: Handling the results of `jot/cut` and `jot/join`.
- **3D (Rule/Boolean)**: Handling apex-to-apex and edge-to-edge contacts in the native kernel.

## 5. Research Findings: CGAL Implementation Path

### A. Combinatorial Vertex Splitting (3D)
The `CGAL/Polygon_mesh_processing/manifoldness.h` library provides the primary tool for resolving bowtie singularities in 3D:
- **`duplicate_non_manifold_vertices()`**: This function identifies "pinched" vertices (where the incident faces form multiple disjoint cycles/umbrellas) and duplicates the vertex for each cycle.
- **Workflow**:
  1. Perform the operation (e.g., `jot/Rule`).
  2. Call `duplicate_non_manifold_vertices(mesh, np::output_iterator(duplicated_groups))`.
  3. The `duplicated_groups` provides a list of `[original_v, new_v1, new_v2, ...]`.
  4. **Geometric Lock**: For each group, displace `new_v_i` by $\epsilon \cdot \vec{N}_i$ (where $\vec{N}_i$ is the average normal of faces incident to that specific vertex copy).

### B. 2D Boolean Singularities
For 2D operations using `General_polygon_set_2` (which uses `Arrangement_2` internally):
- **Coincident Vertices**: `General_polygon_set_2` results may contain multiple `Polygon_with_holes_2` where vertices from different boundaries share a coordinate.
- **Resolution**:
  1. Detect coincident vertices across different polygons in the result set.
  2. For each coincident pair, displace them along the bisector of their incident edges, "shrinking" or "expanding" the polygon slightly to create a gap.

### C. Numerical Robustness
- **Exact Predicates**: Using `EK` (Exact Kernel) allows us to identify *exact* coordinate identity before any floating-point noise is introduced.
- **Subdivision Locality**: To maintain exact planarity, we should split the incident edges at a small distance $\delta$ from the singularity before perturbing the apex. This creates a "transition triangle" that absorbs the non-planarity.

## 6. The Redundancy Paradox (Shared Face Failure)

In 3D boolean kernels (like CGAL's PMP or Corefinement), a common failure mode arises when a mesh contains **redundant connectivity**—specifically, representing the same surface area with multiple overlapping faces (e.g., both a Quad and its Triangulated constituents).

### The Mechanism of Failure
1.  **Over-Sharing**: If an edge is shared by 3+ faces (e.g., two triangles and one quad covering the same area), the mesh is **Non-Manifold**.
2.  **Kernel Rejection**: Most boolean algorithms (union, intersect, cut) strictly require manifold input. Redundant faces cause the kernel to detect self-intersections or "open" boundaries where none physically exist.
3.  **Hole Inversion**: Redundancy can cause the winding-order logic to fail, leading to 3D holes being filled or solids being treated as hollow shells.

### Resolution: Manifold Preservation Protocol
To ensure boolean stability, the 3D bridge (`geometry_to_mesh`) must enforce a "Single Responsibility" rule for every spatial region:
- **Triangle Preference**: If a geometric shape contains both `triangles` (T-code) and `faces` (F-code), the bridge MUST prioritize the triangles and ignore the redundant faces.
- **Deduplication**: Every facet in the CGAL `Surface_mesh` must represent a unique topological boundary.

## 7. Legacy Reference (Implementation Guide)

The `legacy/geometry/repair_util.h` file contains a mature implementation of these strategies:

### `repair_self_touches_xx`
- **Algorithm**:
  1. `duplicate_non_manifold_vertices()` to isolate topological shells.
  2. `CGAL::Euler::split_edge()` to create a local $\delta$-neighborhood.
  3. `CGAL::Euler::split_face()` to maintain valid triangulation of the transition zone.
  4. `vertex_point(v) += average_offset / 2` to geometrically lock the separation.
- **Parameters**: Uses `kIota = 0.0001` as the default subdivision distance.

### `self_touch_util.h`
- **Algorithm**: Provides `make_segment_envelope` and `make_point_envelope` to "promote" zero-volume singularities to non-zero-volume manifold bridges.
