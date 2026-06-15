# Specification: Multi-Dimensional Booleans (`jot/cut`, `jot/join`, `jot/clip`)

## 1. Objective
Extend the JotCAD Boolean system to support a robust, multi-dimensional dispatch matrix. The system must handle interactions between 3D Meshes (Open/Closed), 2D Polygons, 1D Segments, and 0D Points. It also implements a "Ghost Workflow" where tools are preserved for visual reference.

## 2. Fundamental Rules

### A. The Dimensionality Rule
...
### B. Topology: Open vs. Closed
...
### C. The Role System (Disjoint Identifiers)
Every shape carries a `role` tag that defines its participation in the assembly.
- **Solid** (No tag): Standard matter. Participates in all booleans.
- **`gap`**: Subtractive matter. Cuts into solids but cannot be cut.
- **`ghost`**: Visual reference. **Ignored** by all boolean kernels and promoters.
- **`mark`**: Annotation (Ruler, Label). **Ignored** by kernels. Included in PDF export, excluded from STL.
- **`mask`**: Logical boundary. Used for filtering (`keep`/`drop`). Invisible in standard renders.

## 3. The Dispatch Matrix

| Target \ Tool | 3D (Closed) | 3D (Open) | 2D (Polygon) | 1D (Polyline) | 0D (Point) | Plane (Infinite) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **3D (Closed)** | Corefinement | Split/Clip | N/A | N/A | N/A | Plane Clip |
| **3D (Open)** | Corefinement | Corefinement | N/A | N/A | N/A | Plane Clip |
| **2D (Polygon)**| Slice + 2D GPS | Planar Clip | 2D GPS | N/A | N/A | Line/Half-space |
| **1D (Segment)**| Raycast Clip | Surface Clip | Planar Clip | 1D Intersect | N/A | Plane Intersect |
| **0D (Point)**  | In-Volume Test | Side-of-Surf | In-Polygon | Dist-to-Line | Identity | Side-of-Plane |
## 4. Implementation Details

### Automatic Ghosting
When a Boolean operator (`cut`, `clip`, `disjoint`) processes a tool:
1.  The tool's geometry is used for the Boolean calculation.
2.  A copy of the tool is created, tagged with `role: "ghost"` and `opacity: 0.3`.
3.  The operator returns a `Group` containing the result of the calculation and the ghosted tools.

### Kernel Filtering (Promoter Defense)
Promoters like `extrude()`, `fill()`, and `sew()` MUST filter their input.
- Any child component with `role: "ghost"`, `role: "mark"`, or `role: "mask"` is **silently ignored** during the promotion process.
- This prevents a ghosted 2D circle from being "double-extruded" when the group it belongs to is extruded.

### Infinite Plane Tools
...
### 2D Target vs. 3D Tool
- **Algorithm**: `CGAL::Polygon_mesh_processing::split(target, tool)`.
- **Filtering**: Faces of the target are tested against the tool volume using `Side_of_triangle_mesh`. Faces `ON_BOUNDED_SIDE` are removed for `cut`, or kept for `clip`.
- **Result**: Ensures 2D surfaces remain strictly planar after cutting by 3D solids.

### Stamp Operator
- **Algorithm**: Force 3D Corefinement (`corefine_and_compute_difference`) even for 2D targets.
- **Result**: Grafts the tool's boundary onto the surface, creating a 3D embossed shell.

### 3D Target vs. 3D Tool
...

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
