# Dynamic Mesh Operations (Compositions)

Dynamic operations allow users to define new mesh-aware capabilities using Jot expressions. Unlike native C++ primitives, these are "Compositions" executed by a JavaScript evaluator.

## 1. The "Recipe" Model

A Dynamic Operation is a metadata-driven definition that combines a Schema with a Jot Script.

```json
{
  "path": "user/MyBracket",
  "description": "A customizable L-bracket.",
  "inputs": {
    "$in": { "type": "jot:shape" }
  },
  "arguments": [
    { "name": "width", "type": "jot:number", "default": 20 },
    { "name": "thickness", "type": "jot:number", "default": 2 }
  ],
  "script": "$in.cut(Box(width, 10, thickness).at(Corners()))",
  "outputs": {
    "$out": { "type": "jot:shape" }
  }
}
```

## 2. Persistence vs. Content Addressability

- **Definition Store:** Definitions are stored in a persistent, mutable "Project Store" outside the VFS. Standardized keys are used:
    - `jot_user_ops`: The canonical library of User Op scripts.
    - `windows`: Session state including open editor windows and their positions.
    - `desktop`: The dashboard icon layout.
- **VFS Fulfillment:** When a dynamic operation is executed, the *result* is cached in the VFS using the standard **Pure Flat CID** identity rule: `CID = hash(JCB(path, parameters))`.

## 3. Creation & Publishing Workflow

User operations are created and managed through the JOT Editor on the spatial desktop.

1. **Initialization:** Clicking "New Op" creates an untitled draft. It is assigned a temporary internal ID and does not yet exist in the global library or mesh catalog.
2. **Drafting:** The user defines the operator name (case-sensitive) and writes the JOT expression. A **Red Dot** on the publish icon indicates that the current draft differs from the library version.
3. **Publication:** Clicking the **Globe (Publish)** icon in the window titlebar performs the following:
   - Validates the operator name (must follow the `user/name` pattern).
   - Registers the script and schema in the local `jot_user_ops` library.
   - Unifies the window instance with the permanent path.
   - Announces the new capability to the mesh.
   - The indicator turns **Green**, signifying the state is clean.

## 4. Synchronization & Multi-Device Support

JotCAD utilizes a **3-Way Merge Engine** via the RemoteStorage protocol to synchronize your workspace across devices (e.g., Desktop to Mobile).

- **Idle Sync**: To prevent network congestion and unnecessary storage writes, automatic synchronization occurs only after **60 seconds of idle time**, provided a local difference is detected.
- **Manual Trigger**: Users can explicitly trigger a synchronization by clicking the **Sync Cloud** icon on the desktop.
- **Visual Feedback**:
    - **Green Dot**: State is clean and synchronized with the cloud.
    - **Red Dot**: Local changes are pending (Dirty state).
- **Conflict Resolution**: Script merges are performed line-by-line. UI layout merges are ID-aware, ensuring that windows opened on one device appear on others without wiping existing local state.

## 3. Execution & Binding

When a dynamic operation is registered, the host environment (Browser or Node.js) becomes a **Provider** for that path.

1. **Invocation:** A request for `read("user/MyBracket", { width: 30 })` arrives.
2. **Context Binding:** The provider merges the incoming parameters with the argument defaults to create a local variable scope: `{ width: 30, thickness: 2 }`.
3. **Evaluation:** The `JotCompiler` executes the `script` within this bound context.
4. **Fulfillment:** The resulting `Shape` is written to the mesh and returned to the requester.

## 4. Mesh Integration & Catalog

Dynamic operations are first-class mesh citizens:
- **Injection:** Upon creation or startup, definitions are injected into the local `blackboard.schemas` registry.
- **Announcement:** The node broadcasts a `CATALOG_ANNOUNCEMENT` to neighbors.
- **Discovery:** Other nodes (including C++ kernels and remote browsers) see the new operation in their catalog and can invoke it via standard mesh routing.

## 6. Native Operation: Grow (Minkowski Sum)

The `jot/grow` operator implements the Minkowski sum of a subject shape and a tool shape. It utilizes a **Hybrid Minkowski Sum** strategy to preserve interior cavities in concave (non-convex) shapes while maintaining high performance for convex geometries.

### 6.1 Fast Path: The "Single Hull" Strategy
If the subject shape is strongly convex (determined using `CGAL::is_strongly_convex_3`), `jot/grow` calculates the Minkowski sum by:
1. Collecting all vertices of the subject primitive ($V_s$) and the tool ($V_t$).
2. Computing the combined point cloud $V_{sum} = \{ v_s + v_t \mid v_s \in V_s, v_t \in V_t \}$.
3. Generating a single **3D Convex Hull** of the combined set.

### 6.2 Exact Path: Nef Polyhedron Sum (Concave Geometries)
For concave or non-convex shapes, the operator falls back to an exact path using Nef Polyhedra to prevent filling in internal cavities:
1. Converts the subject mesh into an exact 3D Nef Polyhedron (`CGAL::Nef_polyhedron_3<EK>`).
2. Converts the tool shape to a 3D Nef Polyhedron (falling back to its convex hull if it has no geometry).
3. Computes the exact Minkowski sum using `CGAL::minkowski_sum_3`.
4. Converts the resulting Nef polyhedron back to a polygon mesh.
5. Triangulates the faces of the resulting mesh using `CGAL::Polygon_mesh_processing::triangulate_faces` to satisfy the VFS kernel triangulation assertions.
6. If the Nef Minkowski sum fails, it gracefully falls back to the convex hull of the combined point cloud.

### 6.3 Dimensional Integrity
The operator automatically adapts to the dimensionality of the result:
- **3D Solid**: If the summed points form a volume, the result is tagged as `type: "closed"` and returned as a manifold 3D mesh.
- **2D Surface**: If all summed points are coplanar (e.g., 2D subject + 2D tool), the operator extracts the faces of the hull and returns it as a formal `type: "surface"`, avoiding "pancake" instabilities in the 3D unioner.

### 6.3 Mixed-Dimensionality Support
- **Solid Subject + Flat Tool**: Correctly produces a 3D solid (e.g., a $12 \times 12 \times 10$ box from a $10 \times 10 \times 10$ cube grown by a $2 \times 2$ square).
- **Flat Subject + Solid Tool**: Correctly promotes the result to a 3D solid.

## 7. File Format Integration (STL and Wavefront OBJ)

JotCAD supports importing and exporting common 3D mesh formats through standard native operators registered in the VFS.

### 7.1 STL Operators
- **Export (`jot/stl`)**: Exposes the `stl` exporter (lowercase). It traverses the spatial representation of a Shape, triangulates complex non-convex face loops via Constrained Delaunay Triangulation (CDT), and writes a standardized binary STL stream to the `$out` port.
- **Import (`jot/Stl`)**: Exposes the `Stl` constructor (capitalized). It accepts a `jot:file` Selector, reads the raw bytes, auto-detects binary vs. ASCII formatting, parses the facets, and returns a formal CAD Shape of `type: "closed"`.

### 7.2 Wavefront OBJ Operators
- **Export (`jot/obj`)**: Exposes the `obj` exporter (lowercase). It serializes shapes to Wavefront OBJ format, preserving vertex coordinates and face loops.
- **Import (`jot/Obj`)**: Exposes the `Obj` constructor (capitalized). It accepts a `jot:file` Selector, reads the OBJ text stream, parses the 1-based or relative index face patterns, triangulates polygon loops as triangle fans, and materializes the result as a formal CAD Shape of `type: "closed"`.

## 8. Native Operation: Dilate (Convex Solid Offset)

The `jot/dilate` operator implements a fast 3D mitered offset exclusively for strongly convex closed solids by shifting face planes and splitting vertices.

### 8.1 Algorithm
1. **Convex Validation**: Traverses the shape hierarchy, validating that each geometry is strongly convex (via `CGAL::is_strongly_convex_3`) and is a well-formed solid.
2. **Halfspace Plane Shift**: Shifts the plane of each face outward by distance $d = \text{diameter}/2$ along its outward-facing normal.
3. **Halfspace Intersection**: Computes the dual intersection boundary of the shifted planes using `CGAL::halfspace_intersection_3` centered around the centroid of the original shape as the guaranteed interior point.
4. **Face Triangulation**: Triangulates the resulting polygon facets using `CGAL::Polygon_mesh_processing::triangulate_faces` to satisfy VFS triangle mesh requirements.

### 8.2 Schema & Parameters
- **Inputs**: `$in` (The strongly convex solid to offset).
- **Arguments**: `diameter` (The total volumetric expansion width).
- **Outputs**: `$out` (The newly anonymous dilated shape).
