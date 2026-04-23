# Specification: Multi-Dimensional Booleans (`jot/cut`, `jot/join`, `jot/clip`)

## 1. Objective
Extend the JotCAD Boolean system to support a robust, multi-dimensional dispatch matrix. The system must handle interactions between 3D Meshes (Open/Closed), 2D Polygons, 1D Segments, and 0D Points.

## 2. Fundamental Rules

### A. The Dimensionality Rule
**A Tool can only cut or modify a Target of equal or lower dimensionality.**
- **3D Tool**: Cuts 3D, 2D, 1D, 0D.
- **2D Tool**: Cuts 2D, 1D, 0D.
- **1D Tool**: Cuts 1D, 0D.
- **0D Tool**: Cuts only 0D (Point removal).
- **Exceptions**: Points (0D) are cut by everything but can only cut other points.

### B. Topology: Open vs. Closed
- **Closed Mesh (Volume)**: Defines a bounded interior. Cutting a Target by a Volume removes all parts of the Target that fall `INSIDE`.
- **Open Mesh (Surface)**: Does not define a volume. 
  - **Cutting 3D by Open Surface**: Acts as a "Split" or "Clip" operation. The surface acts as an infinite boundary (if planar) or a finite divider.
  - **Orientation**: For open surfaces, the "cut" side is defined by the surface normals (everything on the positive half-space of the normal is removed).

## 3. The Dispatch Matrix

| Target \ Tool | 3D (Closed) | 3D (Open) | 2D (Polygon) | 1D (Polyline) | 0D (Point) | Plane (Infinite) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **3D (Closed)** | Corefinement | Split/Clip | N/A | N/A | N/A | Plane Clip |
| **3D (Open)** | Corefinement | Corefinement | N/A | N/A | N/A | Plane Clip |
| **2D (Polygon)**| Slice + 2D GPS | Planar Clip | 2D GPS | N/A | N/A | Line/Half-space |
| **1D (Segment)**| Raycast Clip | Surface Clip | Planar Clip | 1D Intersect | N/A | Plane Intersect |
| **0D (Point)**  | In-Volume Test | Side-of-Surf | In-Polygon | Dist-to-Line | Identity | Side-of-Plane |

## 4. Implementation Details

### Infinite Plane Tools
A Plane tool represents an infinite half-space boundary. The "cut" side is defined by the plane normal.
- **vs. 3D Mesh**: Use `CGAL::Polygon_mesh_processing::clip` with the plane.
- **vs. 2D Polygon**: 
  1. If the plane is coplanar with the target: No-op or full removal depending on orientation.
  2. If the plane intersects the target plane: The intersection is an infinite line. Use this line to clip the 2D polygon boundaries.
- **vs. 1D Segment**: Calculate the intersection point of the segment and the infinite plane; discard the half-space portion.
- **vs. 0D Point**: Discard points where `plane.has_on_negative_side(p) == false` (keeping only the "bottom" or "back" side).

### 3D Target vs. 3D Tool
- **Algorithm**: `CGAL::Polygon_mesh_processing::corefine_and_compute_difference`.
- **Manifold Defense**: All 3D results MUST pass through `jotcad::geo::fix::make_geometry_unambiguous` to resolve singularities created at the intersection boundaries.

### 2D Target vs. 3D Tool
- **Algorithm**: 
  1. Slice the 3D Tool mesh using the plane of the 2D Target.
  2. The result of the slice is a set of `Polygon_with_holes_2`.
  3. Perform a 2D GPS `difference` between the Target and the Slice-Polygons.

### 2D Target (Polygons/Facets) vs. 2D Tool
- **Algorithm**: Planar 2D Boolean (Exact GPS).
  1. Detect if Target and Tool are coplanar (share a common 3D plane).
  2. Project both onto their common plane's $XY$ space.
  3. Perform 2D `difference` using `General_polygon_set_2`.
  4. Project the result back to 3D.
- **Benefit**: Resolves all numerical instability (gaps/singularities) inherent in 3D-on-3D Booleans for flat objects.

### 1D Target (Segments) vs. 3D Tool
- **Algorithm**: Ray-Mesh intersection using AABB Trees.
  1. Construct an `AABB_tree` of the 3D tool faces.
  2. Intersect each segment with the tree to find split points.
  3. Discard sub-segments where the midpoint is `ON_BOUNDED_SIDE` of the tool.
- **Operators**: 
  - `jot/link`: Creates an open chain of segments.
  - `jot/loop`: Creates a closed loop of segments.

### 0D Target (Points) vs. Any Tool
- **Algorithm**: Spatial membership.
  - Point-in-Volume (3D): `Side_of_triangle_mesh`.
- **Operators**: 
  - `jot/Point`: Single vertex cloud.
  - `jot/Points`: Multi-vertex cloud.
## 5. Representation & Identity

### Exact Ratio Transformation (`tf`)
To ensure absolute stability of the Mesh-VFS identity (CID), transformation matrices are stored as **Exact Ratio Strings** (`"n/d"`).
- **Stability**: Unlike `double` floating-point values, which can have microscopic drift depending on the architecture or platform, ratio strings provide a bit-exact representation for the CID.
- **Precision**: Handled natively by CGAL `FT` on the C++ side and decoded using `BigInt` scaling on the JavaScript side (`ux/src/lib/ft.js`).
- **Compatibility**: The system supports reading legacy `double` arrays but always materializes as ratio strings.

### Visual Documentation (The Gallery)
The system includes a dedicated visual verification suite (`geo/test/boolean_gallery_test.cpp`) that generates "Story" view images.
- **Story View**: A single image showing the `[Target] | [Tool] -> [Result]` sequence.
- **Transparency**: The tool interaction is visualized using an upgraded **RGBA Rasterizer** with alpha-blending, allowing the "Tool" to be rendered as a translucent ghost over the "Target".

## 6. Architectural Updates
1. **`geo/impl/rasterizer.h/cc`**: Upgraded to 4-channel RGBA with back-to-front triangle sorting for transparency support.
2. **`geo/boolean/engine.h`**: Stateless geometric functions for each matrix cell.
3. **`geo/cut_op.h`**: Refactored to perform "Dimensionality Analysis" on the input `Geometry` and dispatch to the engine.
4. **Point Cloud Storage**: Ensure `Geometry` preserves un-indexed vertices for 0D operations.

## 7. Verification Plan
...

- **Test Case 1**: Cut a 3D Cube by a 3D Sphere (Volume-Volume).
- **Test Case 2**: Cut a 2D Square by a 3D Cylinder (Planar-Volume).
- **Test Case 3**: Cut 1D Polylines by a 2D Circle (Segment-Planar).
- **Test Case 4**: Cut 0D Point Cloud by a 3D Box (Point-Volume).
