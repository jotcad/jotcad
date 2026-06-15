# Design Specification: Implicit Displacement Decal

## 1. Overview
The **Implicit Displacement Decal** strategy moves the `jot/decal` operator from a **topological clipping** paradigm to a **field-based displacement** paradigm. Instead of physically cutting and stitching chunks of the relief mesh, we treat the relief as a discrete height function and use it to deform a high-density version of the subject mesh.

## 2. Core Algorithm

### Phase 1: Relief Height Encoding
1.  **Build AABB Tree:** Construct a `CGAL::AABB_tree` of the relief mesh for fast spatial queries.
2.  **Sampling Function:** Define a function $H(u, v)$ that calculates the height of the relief at any 2D coordinate.
    *   Shoot a ray from $(u, v, Z_{max})$ along the $-Z$ axis.
    *   The intersection point with the relief mesh determines the height.
    *   If multiple intersections exist, take the one with the highest $Z$ (top surface).

### Phase 2: Subject Resubdivision
1.  **Target Resolution:** Define a global `resolution` parameter (e.g., 0.1mm).
2.  **Edge Splitting:** Iterate through all edges of the subject mesh. If an edge is longer than the target resolution, split it.
3.  **Refinement:** Perform a density-preserving subdivision (e.g., `CGAL::Subdivision_method_3::PTQ`) to ensure the mesh has enough vertex density to capture the bumps of the relief.
4.  **Normal Calculation:** Ensure per-vertex normals are computed and averaged (using `CGAL::Polygon_mesh_processing::compute_vertex_normals`).

### Phase 3: Triplanar Blended Displacement
For every vertex $V$ in the high-density subject mesh:
1.  **Coordinate Projection:** Map the vertex into three coordinate sets:
    *   $UV_z = (V.x, V.y)$
    *   $UV_x = (V.y, V.z)$
    *   $UV_y = (V.x, V.z)$
2.  **Weight Calculation:** Calculate weights based on the absolute vertex normal $|N|$:
    *   $W_z = |N.z|^p$
    *   $W_x = |N.x|^p$
    *   $W_y = |N.y|^p$
    *   *(Note: $p$ is the blend sharpness, typically 4.0)*. Normalize so $W_x + W_y + W_z = 1.0$.
3.  **Height Blending:** Sample heights from the relief for all three projections and blend them:
    *   $H_{total} = (H(UV_z) \cdot W_z) + (H(UV_x) \cdot W_x) + (H(UV_y) \cdot W_y)$
4.  **Displacement:** Move the vertex along its normal: $V_{new} = V_{old} + (N \cdot H_{total})$.

## 3. Advantages
*   **Topological Robustness:** Because we are only moving existing vertices, we never create new self-intersections, slivers, or non-manifold boundaries. The mesh remains a closed solid.
*   **Seamlessness:** Triplanar blending naturally hides seams at 45-degree angles, creating a perfectly continuous pattern over corners.
*   **Universality:** Works on spheres, toruses, sharp brackets, and branching geometry without any special case logic.

## 4. Implementation Challenges
*   **Performance:** A high-density mesh (e.g., 500k triangles) combined with 3 raycasts per vertex will be slow.
    *   *Optimization:* Cache raycast results in a spatial grid or use a pre-rendered 2D heightmap buffer.
*   **Memory:** Subdivided meshes consume significantly more memory than the original low-poly CAD geometry.
*   **Sharp Edges:** Standard displacement can "round off" sharp CAD edges if the normal averaging isn't carefully handled at crease boundaries.

## 5. Next Steps
1.  Implement `SamplingFunction` using `AABB_tree`.
2.  Integrate a robust resubdivision loop in `decal_pipeline.h`.
3.  Implement the Triplanar Blending vertex-shader-style loop.
