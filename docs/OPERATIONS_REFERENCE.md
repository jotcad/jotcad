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

## 2. Envelopes and Bounds

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

### `boundingBox(grow=0.0)` (alias: `bb()`)
Returns an axis-aligned bounding box for the subject.

- **`grow`**: Optional padding to expand the box symmetrically in all directions.
- **Dimensional Logic**: Returns a Box matching the subject's spatial dimensionality:
  - **3D**: Manifold "closed" solid.
  - **2D**: Planar "surface" face.
  - **1D**: Linear "segments".
  - **0D**: Single "points".
- **Local Origin**: The produced box is generated in local space starting at `(0,0,0)` and transformed to the subject's minimum corner.
- **Example**: `A.by(A.bb().o())` moves shape A so its minimum corner lands at the world origin.

### `orientedBoundingBox(grow=0.0)` (alias: `obb()`)
Returns the optimal (minimum volume) oriented bounding box for the subject.

- **`grow`**: Optional padding to expand the box symmetrically in all directions.
- **Efficiency**: Unlike the standard bounding box, the OBB rotates to find the tightest possible fit for the subject, regardless of its orientation in world space.
- **Matrix**: The resulting shape includes a transformation matrix that aligns the local axes with the OBB's principal directions.
- **Example**: `MyPart.obb().color("cyan")`

## 3. Primitives

Primitives generate basic geometric shapes. Most dimensions support the **`jot:interval`** type, enabling both symmetric and asymmetric positioning.
- **Symmetric**: `10` -> `[-5, 5]` (centered on origin)
- **Asymmetric**: `[10]` -> `[0, 10]` or `[20, 30]` -> `[20, 30]`

### `Box(width=10, height=10, depth=0)`
Generates a 2D rectangle or 3D cuboid.
- **`width`**, **`height`**, **`depth`**: Dimensions as `jot:interval`.

### `Disk(diameter=10, width=0, height=0, start=0, end=1, zag=0.1)`
Generates a circular or elliptical disk/sector.
- **`diameter`**, **`width`**, **`height`**: Dimensions as `jot:interval`. `width`/`height` override diameter if non-zero.
- **`start`**, **`end`**: Angular range in turns (tau).
- **`zag`**: Resolution tolerance.

### `Arc(...)`
Generates a circular or elliptical open arc. Supports multiple construction variants:

#### Variant 1: Bounds-based (Legacy)
`Arc(diameter=10, width=0, height=0, start=0, end=1, zag=0.1)`
- **`diameter`**, **`width`**, **`height`**: Dimensions as `jot:interval`.
- **`start`**, **`end`**: Angular range in turns (tau).

#### Variant 2: 2-Point + Radius (SVG Style)
`Arc(p1, p2, radius, large=false, cw=false, zag=0.1)`
- **`p1`**, **`p2`**: Anchor points (shapes).
- **`radius`**: Arc radius.
- **`large`**: If `true`, uses the large arc (spanning > 180 degrees).
- **`cw`**: If `true`, draws the arc in a clockwise direction.
- **Exact Endpoints**: This variant explicitly aligns vertices to the input anchors to prevent floating-point drift.

#### Variant 3: 3-Point
`Arc(p1, p2, p3, zag=0.1)`
- **`p1`**, **`p2`**, **`p3`**: Points the arc must pass through. `p1` is the start, `p3` is the end, and `p2` defines the curvature and plane.


### `Orb(diameter=10, width=0, height=0, depth=0, zag=0.1)`
Generates a 3D sphere or ellipsoid solid.
- **`diameter`**, **`width`**, **`height`**, **`depth`**: Dimensions as `jot:interval`.

### `Cone(diameter=10, height=10, zag=0.1)`
Generates a sharp cone oriented along the X-axis (Base at X-min, Tip at X-max).
- **`diameter`**: Base diameter as `jot:interval`.
- **`height`**: X-axis extent (length from base to tip) as `jot:interval`.
- **`zag`**: Resolution tolerance.

#### Variant: `Cone/fit(width=10, height=10, depth=10, zag=0.1)`
Generates a cone that fits a specific bounding box.
- **`width`**: X-axis extent (height of cone).
- **`height`**: Y-axis base extent.
- **`depth`**: Z-axis base extent.

#### Variant: `Cone/angle(diameter=10, angle=0.125, zag=0.1)`
Generates a cone with a specific taper angle.
- **`angle`**: Half-angle in turns (e.g., `0.125` is 45 degrees).

### `Triangle(...)`
Generates a 2D triangle. Supports multiple construction variants:

#### Variant 1: Geometric (Side Lengths)
`Triangle(va, vb, vc)`
- **`va`**: Side length along the local X-axis.
- **`vb`**: Side length opposite the first vertex.
- **`vc`**: Side length from the origin to the second vertex.

#### Variant 2: Equilateral (Height)
`Triangle(height=10.0)`
- **`height`**: The vertical distance from the base to the opposite vertex.
- **Logic**: Automatically calculates side lengths for a perfect equilateral triangle.

- **Centering**: All triangles are automatically centered on their **Centroid** at `(0,0,0)`.

### `Hexagon(...)`
Generates a regular 2D hexagon.
- **`turns`**: Rotation in Tau (default `0.0`).

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
- **Disconnected Paths**: Correctly handles disconnected path components by generating separate swept volumes (no "ghost" connections).
- **Junction Handling**: Decomposes complex graphs (Y-junctions, etc.) into simple chains that meet at shared vertices, ensuring full coverage.
- **RMF (Rotation Minimizing Frames)**: Uses the Double Reflection method to ensure the profile does not twist unexpectedly along complex curves.

---

### `stitch(repeat=[10, 3], start=[], end=[], offset=0)`
Applies a recurring on/off length pattern to a segment chain (polylines).
- **`repeat`**: The core pattern applied to the gap between start and end zones. 
- **`start`**: A specific pattern (lengths) applied from the beginning of the path. Alternates ON/OFF.
- **`end`**: A specific pattern (lengths) applied **backward from the end** of the path. Alternates ON/OFF (measured from the tip).
- **`offset`**: Shifts the repeat pattern phase.

## 5. Booleans and Nesting

### `join(tools=[])`
Combines the subject with one or more tool shapes into a single manifold solid. Overlapping volumes are merged.

### `cut(tools=[])`
Subtracts one or more tool shapes from the subject.
- **Dimensionality Aware**: When subtracting a 3D solid from a 2D surface, `cut` strictly produces a flat 2D hole in the surface, adhering to the dimensional plane.
- **Example**: `Box(20, 20).cut(Orb(15))` results in a flat square with a circular hole.

### `stamp(tools=[])`
Topologically embosses the subject using 3D tool shapes.
- **Membrane Effect**: Unlike `cut`, `stamp` grafts the intersecting boundary of the 3D tool onto the subject. If the subject is a 2D surface, it will be "pushed" into the shape of the tool, creating a manifold 3D shell (e.g., a hemispherical depression).
- **Example**: `Box(20, 20).stamp(Orb(15))` results in a flat sheet with a hemispherical "dent" or pocket.

### `clip(tools=[])`
Intersects the subject with one or more tool shapes, keeping only the overlapping volume.

### `fuse(tools=[])`
Flattens a complex assembly into a set of disjoint manifolds. It merges overlapping solids and resolves intersections without removing material (unless `gap()` tags are present).

### `disjoint(tools=[])`
Ensures the subject and tools are topologically disjoint by subtracting the intersection from the subject.

### `pack(parts=null, sheet=null, spacing=2.0, margin=0.0)`
Packs multiple shapes into one or more sheets using a 2D nesting algorithm.
- **`parts`**: A shape or group of shapes to be packed. If omitted, uses the subject.
- **`sheet`**: A shape or group of shapes representing the available material bins.
- **`spacing`**: The minimum distance between parts and the clearance subtracted from the sheet.
- **`margin`**: The minimum distance from the edge of the sheet.
- **Behavior**: Returns an assembly of **Bins**. Each bin is tagged with a numeric `sheet` ID (e.g. `1.0`, `2.0`).
- **Disjoint Geometry**: Each bin contains a **subtracted sheet** as its first component (also tagged with the `sheet` ID). This geometry is the result of subtracting all placed parts from the original sheet, ensuring the material and parts are topologically disjoint.
- **Nesting**: Supports complex geometric nesting, including placing parts inside the holes of other parts.
- **Rigid Items**: If a component is tagged with `"type": "item"` (using `.set("type", "item")`), the packing engine treats it as a single indivisible rigid assembly. The engine will not unpack its children recursively; instead, it packs the entire assembly as one unit. All absolute coordinate transforms (`tf`) within the item are recursively updated, fully preserving local relative translation offsets in absolute space, complying with the **Independent Matrix Mandate**.
- **Example**: `Layout = Parts.pack(sheet=Sheet(48, 96), spacing=0.25)`

### `unfold(rule="grow", minFold=1.0)`
Unfolds a 3D polyhedral mesh into one or more flat 2D patches (islands).
- **`rule`**: The merge algorithm ruleset to apply:
  - **`"grow"`**: Progressively grows islands by merging adjacent co-planar or eligible faces in sequence.
  - **`"pair"`**: Uses a jigsaw merging algorithm that pairs components iteratively.
  - *Note*: `"strategy"` is accepted as a backwards-compatible alias for `"rule"`.
- **`minFold`**: The elasticity/coplanarity threshold in degrees (default `1.0` or $1/360$ of a circle). If the dihedral angle deviation from flat between two adjacent faces is less than `minFold` degrees, the fold/score line is suppressed (omitted from the flat pattern), reflecting material elasticity.
- **Behavior**: Returns a group of flat **Islands** (tagged as `"type": "surface"`). Inside each island group, the output is cleanly isolated into sibling tagged components:
  - **Cut Templates**: The actual flattened faces, tagged with `"unfold": "cut"`.
  - **Fold Lines**: The boundary lines between adjacent folded faces that were not suppressed by `minFold`, preserved as 1D segments and tagged with `"unfold": "fold"`.
- **Progressive Logging**: Outputs live candidate predictions, topological cuts, 2D intersection skips, and high-resolution timing details (elapsed time, candidate averages, ETA) alongside detailed material wastage metrics (solid area vs. bounding box) to `stdout` during execution.
- **Example**: `FlatPattern = Part.unfold(rule="pair", minFold=1.5)`

## 6. Roles and Visibility

The **`role`** tag defines how a shape interacts with the geometric kernel and the visualization engine. Most shapes are physical solids (no role tag), but special cases are explicitly tagged.

### `gap()`
Tags the subject as a **Gap** (Persistent Negative Space).
- **Boolean Logic**: Gaps are "matter-less." They automatically subtract from any non-gap shape they intersect with during `join()`, `clip()`, `fuse()`, or `disjoint()` operations.
- **Persistency**: Gaps are never cut by other shapes.
- **Visuals**: Rendered with **30% opacity** by default.
- **Export**: Excluded from STL/PDF.

### `ghost()`
Tags the subject as a **Ghost** (Visual Reference).
- **Boolean Logic**: Ghosts are "non-matter." They are ignored by all Boolean kernels and promoters (like `extrude` or `fill`). They simply "pass through" other geometry.
- **Usage**: Use for furnishings, reference parts, or "staged" assembly components.
- **Visuals**: Rendered with **30% opacity** by default.
- **Export**: Excluded from STL/PDF.

### `mark()`
Tags the subject as a **Mark** (Annotation).
- **Usage**: Used for rulers, arrows, dimensions, and labels.
- **Boolean Logic**: Ignored by all Boolean kernels.
- **Visuals**: Rendered as **Opaque** by default.
- **Export**: **Included in PDF** drawings but excluded from STL manufacturing files.

### `mask()`
Tags the subject as a **Mask** (Logical Filter).
- **Usage**: Used for defining spatial boundaries or predicates.
- **Logic**: Used with `keep()` and `drop()` for logical filtering without geometric union.
- **Visuals**: **Invisible** in standard renders.

### `clean()`
Recursively removes all components tagged with `role: "ghost"` from the subject tree.
- **Usage**: `Assembly.cut(Drill).clean()` results in the physical part without the ghosted drill reference.

### `opacity(alpha)`
Overrides the visual transparency of a shape.
- **`alpha`**: A number from `0.0` (invisible) to `1.0` (opaque).
- **Note**: This is a visual override and does not affect the shape's geometric role.

## 7. Booleans and Nesting

### `join(tools=[])`
Combines the subject with one or more tool shapes into a single manifold solid. Overlapping volumes are merged.

### `cut(tools=[])`
Subtracts one or more tool shapes from the subject.
- **Automatic Ghosting**: By default, `cut` leaves the tool shapes behind as **ghosts**. This allows you to see the "scaffolding" of your design. Use `.clean()` to remove them.
- **Dimensionality Aware**: When subtracting a 3D solid from a 2D surface, `cut` strictly produces a flat 2D hole in the surface, adhering to the dimensional plane.
- **Example**: `Box(20, 20).cut(Orb(15))` results in a group: `[FlatSquareWithHole, OrbGhost]`.

### `stamp(tools=[])`
Topologically embosses the subject using 3D tool shapes.
- **Membrane Effect**: Unlike `cut`, `stamp` grafts the intersecting boundary of the 3D tool onto the subject. If the subject is a 2D surface, it will be "pushed" into the shape of the tool, creating a manifold 3D shell.
- **Automatic Ghosting**: Like `cut`, `stamp` leaves the tools as ghosts.

### `clip(tools=[])`
Intersects the subject with one or more tool shapes, keeping only the overlapping volume.
- **Automatic Ghosting**: Leaves the tools as ghosts.

### `fuse(tools=[])`
Flattens a complex assembly into a set of disjoint manifolds. It merges overlapping solids and resolves intersections without removing material (unless `gap()` tags are present).

### `disjoint(tools=[])`
Ensures the subject and tools are topologically disjoint by subtracting the intersection from the subject.
- **Automatic Ghosting**: Leaves the tools as ghosts.

### `pack(parts=null, sheet=null, spacing=2.0, margin=0.0)`
...
## 8. Metadata and Filtering

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

## 9. Distribution

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

## 10. Extrusion

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

## 11. Selection and Querying

### `faces()`
Returns a **Generator** of contiguous coplanar patches (Polygons with Holes). It automatically merges contiguous triangles and N-gons into clean surfaces. (Formerly `eachFace`).
- **Anchor**: Each patch is anchored at its **Centroid**.
- **Orientation**: The local **Z-axis** is aligned to the face **Normal**, and the local **X-axis** is aligned to the **first edge** of the outer boundary.
- **Type**: Produced components are tagged as `surface` and have local geometry at $Z=0$.

### `asFaces()`
Returns the subject's faces materialized into a single mesh shape. (Formerly `faces`).

### `edges()`
Returns a generator of all edges as individual oriented components. (Formerly `eachEdge`).
- **Anchor**: Centered at the **Midpoint**, with the Z-axis aligned to the edge normal (interpolated from incident faces).

### `asEdges()`
Returns the subject's edges materialized into a single wireframe shape.

### `corners()`
Returns a generator of all vertices as individual oriented components. (Formerly `eachCorner`).
- **Anchor**: At the **Vertex coordinate**, with the Z-axis aligned to the vertex normal.

### `asCorners()`
Returns the subject's vertices materialized into a single point cloud shape.

### `length()`
**Measurement Operator.**
- **Subject**: A shape with 1D segments (Outlines/Wireframes).
- **Output**: Returns the total Euclidean length of all segments as a `jot:number`.

### `area()`
**Measurement Operator.**
- **Subject**: A shape with 2D or 3D geometry.
- **Output**: Returns the total surface area of the subject as a `jot:number`.

### `volume()`
**Measurement Operator.**
- **Subject**: A shape with a closed 3D shell.
- **Output**: Returns the enclosed volume as a `jot:number`.

### `facing(vector)`
**Measurement Operator.**
- **Subject**: A shape or oriented component.
- **Output**: Returns the **Dot Product** between the subject's local **Z-axis** (normal) and the target vector.
- **Values**: `1.0` (Aligned), `0.0` (Perpendicular), `-1.0` (Opposite).
- **Example**: `faces().highest(facing(UP), 0)` targets the top face.

### `highest(measure, bucket=0, epsilon=null, ratio=null)`
**Selection Operator.**
Ranks the components of a group by a measurement and selects a logically equivalent "bucket" of features, starting from the **highest** value.

- **`measure`**: A partial operator (e.g., `z()`, `area()`, `facing(UP)`). If the measure returns an **Interval** (like `z()`), `highest` implicitly sorts by the **Maximum** value.
- **`bucket`**: The cluster index (0 = Highest cluster).

### `lowest(measure, bucket=0, epsilon=null, ratio=null)`
**Selection Operator.**
Ranks components starting from the **lowest** value. If the measure returns an **Interval**, `lowest` implicitly sorts by the **Minimum** value.

#### Example
```js
// Select the largest face (Highest Area)
MyBox.faces().highest(area(), 0)

// Select the lowest corners (Lowest Z-min)
Table.corners().lowest(z(), 0, ratio=0.01)

// Round only the top-most corners (Highest Z-max)
Box(10).at(corners().highest(z(), 0)).smooth(limit=1/36)
```

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
Box(10).at(corners().highest(z(), 0)).smooth(limit=1/36)
```

## 12. Constants and Directions

To support alignment and selection, the following world-space direction vectors are provided as global constants:

- **`UP`** / **`Z()`**: `[0, 0, 1]`
- **`DOWN`**: `[0, 0, -1]`
- **`RIGHT`** / **`X()`**: `[1, 0, 0]`
- **`LEFT`**: `[-1, 0, 0]`
- **`FRONT`** / **`Y()`**: `[0, 1, 0]`
- **`BACK`**: `[0, -1, 0]`

## 13. Movement

### `m(x, y, z)` / `move(x, y, z)`
Translates the subject by the specified X, Y, and Z offsets.

- **`x`, `y`, `z`**: Numeric offsets.
- **Example**: `Box(10).m(0, 10, 0)`

### `mx(val)`, `my(val)`, `mz(val)`
Axis-specific shorthands for translation. These operators align with the **Universal Sequence Principle**.

- **`val`**: Can be a single `jot:number` or a `jot:numbers` sequence (Array).
- **Sequence Behavior**: If an array is provided, the operator generates a **Group** containing one translated instance for each offset in the sequence.
- **Example**: `Box(10).mx([0, 20, 40])` results in a group of three boxes spaced at 20mm intervals.

## 14. Transformation

### `by(target)`
Transforms the subject's matrix by the matrix of the target shape. 
- **Algebra**: $T_{result} = T_{target} \times T_{subject}$
- **Use Case**: Relative movement or applying a complex frame stored in another object.

### `to(target)`
Moves the subject to the frame of the target shape, resetting any local transformation.
- **Algebra**: $T_{result} = T_{target}$
- **Identity**: `a.to(b)` is equivalent to `a.origin().by(b)`.

### `dup(count=1)`
Duplicates the subject `count` times in place.
- **`count`**: The number of copies to create.
- **Behavior**: Returns the original shape if `count` is 1. Returns a **Group** of identical components if `count` > 1.
- **Use Case**: In-place duplication for subsequent targeted operations (e.g. duplicating a shape before applying different transformations to each copy via `nth`).

### `gap()`
Tags the subject as a **Gap** (Persistent Negative Space).
- **Boolean Logic**: Gaps are "matter-less." They automatically subtract from any non-gap shape they intersect with during `join()`, `clip()`, `fuse()`, or `disjoint()` operations.
- **Persistency**: Gaps are never cut by other shapes. They remain in the shape tree for visual reference but are ignored during `stl()` or `pdf()` export.
- **Visuals**: Gaps are rendered with **30% opacity** in the interactive viewport to distinguish them from solid parts.
- **Example**: `Assembly.join(HolePattern.gap())` ensures clearance holes are maintained regardless of assembly order.

### `origin()` (alias: `o()`)
- **Method**: `X.origin()` or `X.o()`
  Returns the shape with its transformation matrix inverted ($tf = X.tf^{-1}$). This is the primary tool for "un-moving" a shape or calculating an inverse delta.
- **Constructor**: `origin()` or `o()`
  Returns an identity frame (a shape at the world origin with no geometry). Useful as a target for `.to()`.

**Example: Feature Snapping**
To move a shape so its corner lands on the world origin:
```javascript
H = Box(100, 100).mx(50).my(50)
C = H.corners().nth(0)
H.by(C.o()) // "Drags" H by its corner C back to (0,0,0)
```

### `s(x, y, z)` / `scale(x, y, z)`
Scales the subject along the local axes.

- **Involution**: If any scale component is negative (mirroring), the operator automatically flips the face winding to preserve outwards-pointing normals.
- **Sequences**: Supports scale sequences like `sx(1, -1)` to produce "Mirror and Keep" groups.

### `sx(val)`, `sy(val)`, `sz(val)`
Axis-specific shorthands for scaling along X, Y, or Z.

## 15. Mesh Optimization

### `simplify(ratio=0.5, count=0, threshold=1/6)`
Reduces mesh complexity while preserving sharp features using edge-collapse.

- **`ratio`**: Target reduction (e.g., `0.1` keeps 10% of faces).
- **`count`**: Explicit target face count (overrides ratio if > 0).
- **`threshold`**: Dihedral angle (in turns/tau) used to identify and protect sharp features.

## 16. Typography

### `Font(url)`
Downloads and validates a font file (TTF/OTF/WOFF) from a remote URL.

- **`url`**: The full HTTPS/HTTP path to the raw font file.
- **Security**: Validates file headers (magic numbers) to ensure it is a valid font before ingestion.
- **VFS Integration**: Fonts are automatically cached by the VFS; subsequent uses of the same URL are instant.

### `Text(text, font, size=10.0)`
Generates 2D planar geometry (faces) for the specified text string.

- **`text`**: The string of characters to render.
- **`font`**: Either a `Font()` object or a direct URL string.
- **`size`**: The height of the text in world units.
- **Topological Logic**: Uses Parity-based Hole Detection to correctly triangulate characters with negative spaces (like 'A', 'B', 'P', '8').
- **Bezier Subdivision**: Automatically flattens curves into high-resolution polylines (8 segments per curve).

#### Example
```js
// Simple text with default font
Text("JotCAD", size=20)

// Custom font from URL
Text("Hello", font="https://example.com/MyFont.ttf", size=50).ez(5)
```

### `Image(url)`
Downloads and validates a raster image (PNG/JPG) from a remote URL.

- **`url`**: The full HTTPS/HTTP path to the raw image file.
- **Security**: Validates file headers (magic numbers) to ensure it is a valid image before ingestion.
- **VFS Integration**: Images are automatically cached by the VFS; subsequent uses of the same URL are instant.

### `Trace(image, colors=8, smooth=1.0)`
Converts a raster image into vector geometry using automatic color quantization.

- **`image`**: An `Image()` object or direct URL to a bitmap.
- **`colors`**: Number of color buckets to automatically identify using K-Means clustering.
- **`smooth`**: Simplification tolerance (Douglas-Peucker). Higher values result in fewer vertices and smoother curves.
- **HSV Quantization**: Automatically identifies the most dominant colors in the image using HSV space, preserving color separation across varying brightness.
- **Boundary Injection**: Automatically pads image edges to ensure large regions (like the sky) are correctly closed into manifold loops.
- **Despeckling**: Includes a noise-reduction pass and area-based filtering to eliminate small, jagged "speckle" polygons.
- **Output**: A `Group` containing one child shape for each identified color. Each child is tagged with its average hex color.

#### Example
```js
// Automatically trace a landscape with 12 colors and high smoothing
Photo = Image("landscape.jpg");
Vector = Trace(Photo, colors=12, smooth=2.0);

// Select and extrude the specific color region
Vector.on("#bcd3ee").ez(5);
```

### `Relief(image, width=10.0, height=10.0, depth=2.0, base=1.0, resolution=128)`
Generates a 3D relief mesh from a 2D grayscale/bump map image.

- **`image`**: An `Image()` object or direct URL to a bitmap.
- **`width`**: Physical target width of the relief shape along the X-axis.
- **`height`**: Physical target height of the relief shape along the Y-axis.
- **`depth`**: Maximum extrusion height (along the Z-axis) corresponding to white pixels.
- **`base`**: Thickness of the base bottom plate (below the heightmap surface).
- **`resolution`**: The maximum sampling grid resolution (clamped to the image dimension). Controls triangle density to keep computations fast.
- **Solid Closure**: Automatically seals the mesh by adding a flat bottom plane ($Z = 0.0$) and vertical side-wall skirting to produce a watertight manifold suitable for 3D printing.

#### Example
```js
// Load heightmap
Map = Image("heightmap.png");

// Generate a 100x100mm relief, 5mm max displacement, with a 2mm base
ReliefMesh = Relief(Map, width=100, height=100, depth=5, base=2, resolution=256);

// Export to STL
ReliefMesh.stl("model.stl");
```

### `Conform(target, direction=[0,0,0], offset=0.0)`
Wraps or projects the subject geometry onto a target surface. 
- If `direction` is `[0,0,0]`, it performs a **closest-point (shrink-wrap)** projection.
- If a `direction` is provided, it performs a **directional (raycast)** projection.
- `offset` specifies the distance to maintain from the target surface.

## 17. Infinite Planes (Orientations)
...
- **`X(offset=0)`**: Infinite plane on the YZ axis (normal +X).
- **`Y(offset=0)`**: Infinite plane on the XZ axis (normal +Y).
- **`Z(offset=0)`**: Infinite plane on the XY axis (normal +Z).

#### Example
```js
// Extrude along the world X axis
Box(10).at(X()).extrude(5)
```
