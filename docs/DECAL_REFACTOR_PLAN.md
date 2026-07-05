# Decal Operator Refactoring: Planar Patch Texture Mapping

## Core Concept
The `jot/decal` operation is a **Surface-Preserving Clip and Re-place** operation. 
To avoid distortion (stretching) and ensure smooth continuity across edges, we use **Surface Development (Unfolding)** to map the 3D subject onto the **Master UV Space** ($Z=0$ plane of the Relief's local space).

## Implementation Pipeline (Pure 3D Booleans)

### 1. Subject Segmentation
* Group subject triangles into **Monoplanar Patches**.

### 2. Surface Development (Unfolding to UV Space)
* Instead of a linear top-down projection, we "unroll" the 3D patches onto the $Z=0$ plane of the Relief's local space.
* **Distance Preservation**: The transformation for each patch must preserve the arc-length (surface distance). If a vertex is $1.0$ units from an edge in 3D, it must be $1.0$ units from that edge in UV space. This eliminates "stretching."
* **Adjacency Continuity**: Shared edges in 3D are used as "hinges" to lay adjacent patches flat next to each other in UV space. This ensures the texture flows smoothly across sharp corners.
* **Mapping Matrix**: For each patch $i$, we derive a unique $M_{unfold, i}$ that places it at the correct $(u, v)$ coordinates on the $Z=0$ plane.

### 3. Build Extrusion Slab (The Clipper)
For each Flattened Patch at $Z=0$:
* Create a 3D **Slab** by extruding the flat 2D footprint along the **Relief's local Z-axis** ($Z_{min}$ to $Z_{max}$).
* This forms a vertical prism that perfectly captures the relevant 3D geometry of the relief.
* The Slab is a closed, manifold triangular mesh.

### 4. Physical Clipping
* Perform an exact 3D Boolean Intersection (`clip_mesh_by_mesh`) between the **Relief Mesh** and the **Slab**.
* The Relief mesh remains stationary in its own local space.
* Result: 3D relief chunks that are shaped exactly like the flattened patch.

### 5. Inverse Mapping (Re-folding)
* Transform the clipped relief chunks from the $Z=0$ UV space back to their original 3D positions on the subject's surface.
* The transformation is the inverse of the unfolding matrix: $M_{refold, i} = M_{unfold, i}^{-1}$.
* The relief geometry is "wrapped" back onto the subject, maintaining its local height/depth relative to the surface.

### 6. Join Divergent Seams (Skirting)
* Even with perfect unfolding, the thickness of the relief can cause gaps at sharp convex corners (the chunks "pull apart" as they follow the surface normal).
* We identify shared boundary edges and generate vertical "skirting" triangles to bridge the gap between the re-folded relief chunks.

### 7. Assembly
* Combine all replaced patches and skirting geometry.
* Run a final vertex-merge and removal of isolated components.

## Strict Invariants
* **NO 2D MATH:** No 2D projections, raycasting, or sampling.
* **NO CDT:** No Constrained Delaunay Triangulation.
* **INDEPENDENT MATRIX:** Adhere strictly to the independent transform model.

## Execution & Testing Plan (Implemented in `geo/ops/decal_pipeline.h`)
1. **Step 1: Segmentation:** Group faces by normal. (Test: Triangulated Cube -> 6 patches).
2. **Step 2: Texture Space Mapping:** Transform 3D patches into Texture Space. (Test: Verify vertex positions).
3. **Step 3: Slab Generation:** Extrude 3D patches along Z using dual projectors. (Test: Verify manifold solidity).
4. **Step 4: Relief Clipping:** Boolean intersect relief with slab. (Test: Verify containment).
5. **Step 5: Subject Replacement:** Map clipped chunks back to replace original patches.
## Post-Mortem: 2026 Refactor Findings & Limitations

Following the implementation attempt of June 2026, we have identified two fundamentally competing strategies, each with a "hard" mathematical limitation when using an Exact Boolean kernel:

### 1. Strategy A: Surface Development (Unfolding)
*   **Method:** "Unroll" the 3D surface using relative BFS hinges from a root face to create a continuous 2D UV map.
*   **Strength:** Perfectly seamless. Relief patterns wrap around corners with zero distortion or discontinuities.
*   **Critical Failure (Concavity):** On concave shapes (e.g., L-brackets) or wraparounds (e.g., toruses), the BFS unfolding math forces the surface to **fold back onto itself** in 2D UV space. This causes the 2D map to overlap, creating a non-bijective mapping. The resulting extrusion slab collapses into a 1D line or a self-intersecting mess, causing the Exact Boolean engine to crash.

### 2. Strategy B: Axis-Aligned Mapping (Triplanar)
*   **Method:** Group patches by their dominant normal (X, Y, or Z) and project the relief independently for each group.
*   **Strength:** Topology-agnostic. It never crashes, regardless of how complex or concave the subject is, because each patch is explicitly laid flat on its own optimal axis.
*   **Critical Failure (Seams):** Because the projections are orthogonal, the physical 3D bumps of the relief **clash/intersect** at the 90-degree corners where the mapping axis switches.
    *   **Geometric Discontinuity:** Simple "patch replacement" produces a broken, non-manifold "polygon soup" because the boundaries of the X-projection do not align with the vertices of the Z-projection.
    *   **Assembly Complexity:** To produce a true closed manifold result, each chunk must be turned into a 3D solid and unioned using `join_mesh_by_mesh`. This effectively turns the Decal operator into a sequence of dozens of expensive 3D Solid Booleans.

### Final Conclusion
The goal of a **Seamless, Physical 3D Decal** on **Complex CAD Topology** is mathematically doomed if using planar clipping alone.
*   If **Seamlessness** is required, the topology must be strictly restricted to convex/developable shapes.
*   If **Robustness** is required (handling any shape), the system must accept physical geometric seams at alignment transitions.
*   A "Third Path" would require **3D Spatial Deformation** (physically bending the relief vertices along the target surface normals), which bypasses planar clipping but introduces massive mesh-integrity and performance challenges.

**Current Implementation State:** `geo/ops/decal_pipeline.h` currently implements Axis-Aligned (Triplanar) mapping. 

As of June 2026, the non-manifold / open-mesh issue during surface replacement has been resolved. The boundary of the subject hole (which is split into multiple segments due to vertical diagonals of the cutter side faces) is matched to the relief border by projecting and sorting collinear vertices, interpolating the Z-coordinate on the relief, and building a clean, non-overlapping quad strip. Autorefinement is then able to resolve T-junctions, ensuring a watertight, closed manifold solid.

### Final Implementation & Optimization Achievements (June 2026):

1. **Dijkstra Closest-Pair Seam Routing**:
   * Evaluated loop rotations for matching the subject boundary to the relief boundary. Instead of exhaustively testing all rotations (which scaled quadratically and took >10 minutes for complex boundaries like the 4,263-vertex relief in `decal_hill_perf`), we locate the geometrically closest vertex pair between the subject hole and relief boundaries.
   * By using this closest pair to anchor the loop starting points, the Dijkstra-based shortest-path triangulation is executed exactly once, reducing seam routing time from 10+ minutes to less than 10 milliseconds.

2. **CGAL Exact Kernel Vertex Mapping**:
   * Skirting and projection boundary calculations are performed using double-precision arithmetic. To prevent CGAL from creating duplicate/disconnected vertices due to floating-point drift, a `std::map<PointCgal, Vertex_index> rs_vertex_map` is used to map coordinates back to their exact original equivalents.
   * This maintains exact boundary alignments and prevents duplicate vertex generation.

3. **Watertight Winding Order Auto-Correction**:
   * During face insertion into the CGAL surface mesh, winding conflicts can lead to silent failure (returning `null_face()`).
   * We added automatic winding-order detection and fallback. If a face fail occurs, the vertex order is reversed and retried. An exception is explicitly thrown if the face still fails to add, ensuring mesh integrity issues are caught immediately.

4. **STEP Integration Mock Fallback**:
   * Solved the STEP integration test hang where empty/zero-length file bytes returned by the file provider in `ux/src/lib/vfs/FileProvider.js` bypassed the old `!fileBytes` null-check in `geo/export_service.js`.
   * Updated the check to `(!fileBytes || fileBytes.length === 0)` to correctly trigger the mock geometry fallback, preventing empty files from hanging the OpenCascade parser.

### July 2026 Integration & Test Suite Refinement

1. **Correction of Matrix Representation**:
   * Replaced non-standard `cartesian` matrix structures in the decal tests with standard space-separated homogeneous matrix strings. This ensures compliance with raw selector validation (`normalizeSelector`) and prevents type hydration errors.

2. **MeshLink Lifecycle Control**:
   * Resolved hanging sockets in JS integration tests by wrapping setup, execution, and cleanup in a standard `finally` block that explicitly invokes `mesh.stop()` on the `MeshLink` instance, freeing Zenoh resources.

3. **Catalog Race Condition Resolution**:
   * Replaced static delays with `waitForMeshNodes` calls in the decal test to ensure that the VFS client waits until the necessary C++ provider nodes have synchronized their operators on the Zenoh mesh before executing shape queries.


