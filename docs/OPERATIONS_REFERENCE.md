# JotCAD Operations Reference

This document provides a detailed reference for the built-in geometric operations available in the JotCAD DSL.

## 1. Path Construction

### `Link($in, tools, smooth=false, zag=0.0)`
Connects a series of points into an open path.

- **`$in`**: The primary shape (subject). Its vertices are used as the starting points.
- **`tools`**: A list of additional shapes. The vertices of these shapes are appended to the path in order.
- **`smooth`**: If `true`, applies Catmull-Rom interpolation to create a curved path.
- **`zag`**: Controls the resolution of the smooth curve (e.g., `0.1` results in 10 steps per segment).

### `Loop($in, tools, smooth=false, zag=0.0)`
Connects a series of points into a closed loop.

- **`$in`**, **`tools`**, **`smooth`**, **`zag`**: Same as `Link`, but the final point is automatically connected back to the first point to close the loop.

## 2. Hull (Convex Envelopes)

### `Hull(shapes=[])` / `hull()`
Generates the convex hull of the input shape(s).

- **2D Hull**: If all vertices lie on a single plane, generates a 2D face representing the planar convex hull.
- **3D Hull**: If vertices are not coplanar, generates a manifold 3D solid representing the volumetric convex hull.
- **Recursion**: Collects vertices from all child components and applies their relative transforms.

#### Example
```js
// 3D Volumetric Hull
Hull(Cylinder(5), Box(10).m(20))

// 2D Method call
MyPath.hull().fill()
```

## 3. Primitives

### `Hexagon(...)`
Generates a regular 2D hexagon. The operator supports multiple dimensioning variants, which are automatically selected based on the provided arguments.

#### Common Arguments
- **`turns`**: Rotation in Tau (default `0.0`). `0.0` is flat-topped; `1/12` (30 deg) is pointy-topped.

#### Variants
| Variant | Argument | Description | VFS Path |
| :--- | :--- | :--- | :--- |
| **Radius** | `radius` | Center-to-corner distance. | `jot/Hexagon/radius` |
| **Diameter** | `diameter` | Corner-to-corner distance (diagonal). | `jot/Hexagon/diameter` |
| **Apothem** | `apothem` | Center-to-flat distance. | `jot/Hexagon/apothem` |
| **Edge-to-Edge** | `edgeToEdge`| Flat-to-flat distance. | `jot/Hexagon/edgeToEdge` |
| **Diagonal** | `diagonal` | Alias for diameter. | `jot/Hexagon/diagonal` |
| **Edge-Length** | `edgeLength`| Length of a single side. | `jot/Hexagon/edgeLength` |

#### Example
```js
// Flat-topped hexagon by radius
Hexagon(radius=10)

// Pointy-topped hexagon by edge-to-edge distance
Hexagon(edgeToEdge=20, turns=1/12)
```

## 4. Promoters (Dimensional Upgrades)

### `fill(rule='odd', plane=null)`
Promotes 1D segments (Paths/Outlines) into 2D areas (Faces).

- **`rule`**: The topological filling rule.
  - **`odd`** (Default): Even-Odd parity. Fills alternating regions (standard holes).
  - **`any`**: Non-zero winding. Fills any region with a boundary.
  - **`pos`**: Positive winding only.
  - **`neg`**: Negative winding only.
  - **`all`**: All-Bounded. Fills every enclosed cell regardless of winding.
- **`plane`**: An optional orientation plane. If omitted, the operator infers the plane from the input segments or falls back to the local workbench (XY).

#### Example
```js
// Convert a loop into a face
Loop(Point(0,0), Point(10,0), Point(0,10)).fill()

// Fill a complex wireframe using the 'all' rule
MyWireframe.fill(rule='all')
```

### `Sew(shapes=[])` / `sew(shapes=[])`
Stitches independent 2D faces into manifold shells or 3D solids.

- **`shapes`**: A list of additional shapes to weld into the assembly.
- **Topological Logic**:
  - **Vertex Welding**: Merges vertices with identical rational coordinates.
  - **Manifold Guard**: Fails if an edge is shared by more than 2 faces (no self-touches).
  - **Orientation Solver**: Consistently orients connected components.
- **Biasing**:
  - **Closed Volumes**: Normals point **outward** (positive volume).
  - **Open Shells**: Normals bias toward **+Z** (Floor-Up).

#### Example
```js
// Stitch faces into a solid
Sew(Front, Back, Top, Bottom, Left, Right)

// Add a patch to an existing shell
MyShell.sew(Patch)
```

### `Sweep(profiles, path, closed_path=false, solid=true)` / `sweep(path, closed_path=false, solid=true)`
Extrudes one or more 2D profiles along a 3D path.

- **`profiles`**: A list of 2D shapes to sweep.
- **`path`**: A shape containing 1D segments defining the trajectory.
- **`closed_path`**: If `true`, connects the end of the sweep back to the start with twist correction.
- **`solid`**: If `true`, generates a manifold solid with start/end caps. If `false`, generates a wireframe/surface assembly.
- **RMF (Rotation Minimizing Frames)**: Uses the Double Reflection method to ensure the profile does not twist unexpectedly along complex curves.

#### Example
```js
// Create a pipe
Sweep(profiles=[Circle(5)], path=CubicPath(), solid=true)

// Sweep a square along a closed loop
Square(2).sweep(path=MyLoop, closed_path=true)
```

## 5. Export and Rendering

### `pdf(path='export.pdf')`
Generates a PDF document from the spatial representation of the input shape.

- **`path`**: The filename for the download.
- **Outputs**:
  - **`$out`**: The input shape (pass-through).
  - **`file`**: The generated PDF blob (triggers download in the UX).

### `png()`
Generates a PNG thumbnail for the input shape.

- **Outputs**:
  - **`$out`**: The input shape (pass-through).
  - **`file`**: The generated PNG blob.

### `section(planes=[])`
Extracts 2D cross-sections of the input shape at specified planes.

- **`planes`**: A list of shapes whose transforms define the sectioning planes.
- **Default**: If no planes are provided, performs a section at local $Z=0$.
- **Logic**: Slices the input geometry and merges overlapping/nested loops into clean 2D faces.

### `separate()`
Splits the input geometry into separate shapes based on connected components (disconnected geometric islands).

- **Output**: A group containing one child shape for each disconnected piece of geometry.
- **Support**: Works for 3D solids, 2D faces, 1D segments, and 0D points.
- **Usage**: Useful after boolean operations (like `cut`) that result in multiple disjoint parts.

#### Example
```js
// Cut a box in half and treat the two halves as separate objects
Box(10).cut(Plane().m(0, 0, 5)).separate()
```

## 6. Plane and Normal Extraction

### `plane()`
Extracts the coordinate system from the first face of a shape.

- **Subject**: A shape with 2D or 3D geometry.
- **Output**: A new shape with **no geometry** but a transformation matrix where:
  - **Origin**: The first vertex of the first face.
  - **Z-Axis**: The normal vector of the face.
- **Tags**: `type: "plane"`, `is_plane: true`.
- **Usage**: Used to re-orient the workbench to a face for further operations like `at()` or `on()`.

### `normal()`
Extracts the normal vector of the first face as an oriented normal shape.

- **Subject**: A shape with 2D or 3D geometry.
- **Output**: A shape with a single unit segment pointing along the face's normal.
- **Tags**: `type: "normal"`.
- **Usage**: Typically used as an input to extrusion or directional translation: `MyShape.at(face.normal()).extrude(10)`.

### `outline()`
Extracts the wireframe outlines of the subject.

- **Feature Extraction**: Only keeps boundary edges (shared by 1 face) or feature edges (shared by 2 faces with different normals).
- **Coplanar Filter**: Automatically ignores edges between coplanar faces (zero-degree bends) to produce clean silhouettes.

## 7. Distribution

### `place(template_shape)`
Instantiates a template shape at every anchor point in the subject collection.

- **Iterative Pattern**: Used with selection generators like `eachCorner()` or `eachFace()`.
- **Composition**: Each instance preserves the `template_shape` transforms while being placed on the target anchor.

#### Example
```js
// Put a washer on every corner
Plate.eachCorner().place(Disk(2))

// Instantiate bolts into a hole pattern
Pattern.place(Bolt())
```

## 8. Extrusion

### `e(target)` / `extrude(target)`
The primary tool for promoting geometry along a vector or between coordinate systems.

- **Normal Sweep (Number)**: If `target` is a number, the operator calculates the face normal of the subject and extrudes along it by that distance.
  - Example: `Box(10).e(5)` (Extrudes 5 units along the local normal).
- **Transform Sweep (Shape)**: If `target` is a shape, the operator sweeps the geometry from its current frame to the coordinate system of the target shape.
  - Example: `Box(10).e(Z(20))` (Sweeps to the plane at world Z=20).
- **Promotion Rules**:
  - **Faces $\rightarrow$ Solids**
  - **Edges $\rightarrow$ Faces**
  - **Points $\rightarrow$ Edges**

### `ex(height)`, `ey(height)`, `ez(height)`
Axis-specific shorthands that take a numeric height and extrude along the local X, Y, or Z axes respectively.

- Example: `Box(10).ez(5)` is equivalent to `Box(10).e(Z(5))`.

### `spin(start=0, end=1, resolution=32)`
Revolves geometry around the local Z axis to create circular features or solids of revolution.

- **`start`**: Initial angle in turns (default `0.0`).
- **`end`**: Final angle in turns (default `1.0` for a full circle).
- **`resolution`**: Number of angular segments.
- **Capping**: If the spin is not a full circle (`end - start != 1.0`), the result is automatically capped to form a valid solid.
- **Promotion**: Points $\rightarrow$ Arcs, Segments $\rightarrow$ Ribbons, Faces $\rightarrow$ Solids.

### `spx(start, end)`, `spy(start, end)`, `spz(start, end)`
Axis-specific shorthands for revolving around the local X, Y, or Z axes.

## 9. Selection and Querying

### `faces()`
Returns a collection of all faces in the subject as individual shapes.

### `eachFace()`
Returns a **Generator** of all faces. When used with `at()` or `on()`, it sequentially targets every face of the subject.
- **Anchor**: Each face is anchored at its **Centroid**, with the local Z-axis aligned to the face **Normal**.

### `eachEdge()`
Returns a generator of all edges.
- **Anchor**: Centered at the **Midpoint**, with the Z-axis aligned to the edge normal (interpolated from incident faces).

### `eachCorner()`
Returns a generator of all vertices.
- **Anchor**: At the **Vertex coordinate**, with the Z-axis aligned to the vertex normal.

### `smooth(limit=1/24, iterations=20, resolution=1.0, region=null)`
Smooths geometry until dihedral angles approach a specified limit.

- **`limit`**: Target dihedral angle in turns (tau). Corners sharper than this will be rounded.
- **`iterations`**: Max number of relaxation steps.
- **`resolution`**: Subdivision factor. Values $> 1.0$ will refine the mesh to support tighter curves.
- **`region`**: Optional bounding shape. If provided, only vertices within this volume are smoothed.

#### Example
```js
// Round all edges to 15 degrees (1/24 turns)
Box(10).smooth(limit=1/24, resolution=2.0)

// Round only the top-most corners
Box(10).at(eachCorner().top()).smooth(limit=1/36)
```

## 10. Transformation

### `s(x, y, z)` / `scale(x, y, z)`
Scales the subject along the local axes.

- **Involution**: If any scale component is negative (mirroring), the operator automatically flips the face winding to preserve outwards-pointing normals.
- **Sequences**: Supports scale sequences like `sx(1, -1)` to produce "Mirror and Keep" groups.

### `sx(val)`, `sy(val)`, `sz(val)`
Axis-specific shorthands for scaling along X, Y, or Z.

## 11. Mesh Optimization

### `simplify(ratio=0.5, count=0, threshold=1/6)`
Reduces mesh complexity while preserving sharp features using edge-collapse.

- **`ratio`**: Target reduction (e.g., `0.1` keeps 10% of faces).
- **`count`**: Explicit target face count (overrides ratio if > 0).
- **`threshold`**: Dihedral angle (in turns/tau) used to identify and protect sharp features.


## 12. Infinite Planes (Orientations)
...
- **`X(offset=0)`**: Infinite plane on the YZ axis (normal +X).
- **`Y(offset=0)`**: Infinite plane on the XZ axis (normal +Y).
- **`Z(offset=0)`**: Infinite plane on the XY axis (normal +Z).

#### Example
```js
// Extrude along the world X axis
Box(10).at(X()).extrude(5)
```
