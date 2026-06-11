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
6. **Step 6: Seam Joining:** Stitch divergent edges at patch boundaries.
