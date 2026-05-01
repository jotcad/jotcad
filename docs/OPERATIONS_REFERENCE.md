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

## 2. Point Collection Rules

Both `Link` and `Loop` use a recursive vertex collection strategy:
- If a shape contains **Geometry**, all of its vertices are harvested.
- If a shape has no geometry (like a container or a `Point()`), its **Origin** (0,0,0) in local space is used.
- All points are transformed into the world space of the operation before connection.

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

## 7. Infinite Planes (Orientations)

These operators provide standard orthogonal orientations. They are often used as "World Workbenches".

- **`X()`**: Infinite plane on the YZ axis (normal +X).
- **`Y()`**: Infinite plane on the XZ axis (normal +Y).
- **`Z()`**: Infinite plane on the XY axis (normal +Z).

#### Example
```js
// Extrude along the world X axis
Box(10).at(X()).extrude(5)
```
