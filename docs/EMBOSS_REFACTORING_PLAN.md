# Emboss Refactoring Plan: Surface Corefinement & Depth-Map Projection

This document outlines the architectural plan to refactor the `jot/emboss` operator. The goal is to move away from solid-solid 3D boolean operations and instead use **topological corefinement, local region splitting, adaptive subdivision, and projective displacement mapping**.

---

## 1. Objectives & Limitations of Current Approach

### Current Approach
* Creates a separate coarse 3D tool volume by projecting original pattern vertices onto the surface and extruding.
* Executes a solid-solid boolean union/difference via `join_mesh_by_mesh` / `cut_mesh_by_mesh`.

### Limitations
1. **Booleans are Fragile**: Nef Polyhedron booleans often fail or crash on coplanar faces or self-intersections of the projected tool.
2. **No Curvature Tracking**: Coarse edges between pattern vertices remain straight, cutting through or floating above curved target surfaces (chordal error).
3. **No Depth-Map Support**: Cannot displace based on grayscale textures/heightfields at high resolution.

---

## 2. Corefinement & Splitting Architecture

To solve these limits, the refactored operator will follow a **surface-grafting** pipeline:

```
[Input Subject Mesh & Depth Map]
               │
               ▼
   [Define Texture Projector]
               │
               ▼
[Project Boundary & Corefine Target Mesh]
               │
               ▼
[Split Mesh into Outside Subject & Inside Patch]
               │
               ▼
       [Subdivide Patch]
               │
               ▼
[Sample Depth Map & Displace Vertices along Normals]
               │
               ▼
[Duplicate Seam Vertices & Generate Stitching Skirting]
               │
               ▼
  [Combine Watertight Surface Mesh]
```

### Key Stages

1. **Texture Projection Mapping**:
   Map 3D surface vertices $(X, Y, Z)_{\text{world}}$ to 2D texture coordinates $(u, v) \in [0, 1]^2$ based on a Planar Projector.
2. **Mesh Splitting (CGAL PMP)**:
   Embed the rectangular boundary of the projector onto the target surface using `CGAL::Polygon_mesh_processing::split()`.
3. **Local Patch Subdivision**:
   Tessellate the inside patch to ensure enough vertex density is present to resolve the height variations in the depth map.
4. **Vertex-Level Normal Displacement**:
   Displace each patch vertex along its local face normal based on the sampled depth value:
   $$Z_{\text{displacement}} = \text{offset} + \text{DepthMap}(u, v) \times \text{depth}$$
5. **Seam Stitching**:
   Duplicate boundary vertices along the split loop, shift the patch boundary, and generate a watertight strip of vertical triangles to bridge the gap.

---

## 3. Detailed Implementation Steps

### Phase 1: Projector Mapping & Image Ingestion
* Add a `projector` argument to `jot/emboss` (e.g., standard mapping frame or coordinate system).
* Load the depth map bytes using `stbi_load_from_memory` inside `EmbossOp`.
* Implement a `PlanarProjector` helper:
  ```cpp
  struct PlanarProjector {
      Matrix world_to_proj; // Projects 3D world space onto the 2D texture plane
      std::pair<double, double> map(const IK::Point_3& p) const {
          // Transforms point to local plane space and returns (u, v)
      }
  };
  ```

### Phase 2: Seam Insertion & Mesh Splitting
* Generate a closed boundary polyline (rectangle) in 3D representing the texture's bounds projected onto the target mesh.
* Use `CGAL::Polygon_mesh_processing::split()` to insert these boundary edges into the target mesh.
* Partition target faces into `inside_faces` (projected region) and `outside_faces` using `CGAL::Side_of_triangle_mesh` or a flood-fill from the projector center.

### Phase 3: Adaptive Local Tessellation
* Refine the isolated inside patch to match the depth map resolution:
  * For each face in `inside_faces`, if its edges exceed the pixel spacing size $\delta = \max(\text{width}/\text{res\_x}, \text{height}/\text{res\_y})$, subdivide the face.
  * Use local edge splitting (`CGAL::Euler::split_edge`) to build density while maintaining a valid triangulation.

### Phase 4: Normal Displacement & Duplicate Welds
* Duplicate the boundary loop vertices to separate the patch from the outer mesh.
* For each vertex $v$ inside the patch:
  * Compute $(u, v)$ coordinates.
  * Sample grayscale intensity $I(u, v)$ from the depth map.
  * Calculate displacement: $d = \text{offset} + I(u, v) \times \text{depth}$.
  * Update vertex coordinate: $P_{\text{new}} = P_{\text{old}} + N \times d$ (where $N$ is the surface normal).
* Generate triangles between the original boundary vertex loop and the displaced boundary vertex loop.

### Phase 5: Watertight Assembly
* Join the outside faces, the vertical side skirting, and the displaced inside faces back into a single `Surface_mesh`.
* Run mesh cleaning (`CGAL::Polygon_mesh_processing::remove_isolated_vertices`) and verify manifold soundness.

---

## 4. Verification & Testing Strategy

To verify this implementation, we will add a new test suite `emboss_projection_test.cpp`:
1. **Target**: A curved cylinder or sphere shape.
2. **Pattern**: A custom 2D grayscale gradient texture (depth map).
3. **Execution**: Project the texture onto the side of the cylinder and emboss.
4. **Validation**:
   * Verify the boundary is perfectly clean (corefined).
   * Verify that only the textured region is subdivided, keeping the ends of the cylinder at low polygon counts.
   * Generate a verification image using `jot/png` to inspect the conformant 3D relief visually.
