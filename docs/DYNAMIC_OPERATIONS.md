# Dynamic Mesh Operations (Compositions)

Dynamic operations allow users to define new mesh-aware capabilities using Jot expressions. Unlike native C++ primitives, these are "Compositions" executed by a JavaScript evaluator.

## 1. The "Recipe" Model

A Dynamic Operation is a metadata-driven definition that combines a Schema with a Jot Script.

```json
{
  "path": "user/MyBracket",
  "description": "A customizable L-bracket.",
  "arguments": {
    "width": { "type": "number", "default": 20 },
    "thickness": { "type": "number", "default": 2 }
  },
  "script": "Box(width, 10, thickness).cut(Box(5, 5, thickness).at(Corners()))",
  "outputs": {
    "$out": { "type": "shape" }
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

The `jot/grow` operator implements the Minkowski sum of a subject shape and a tool shape. It utilizes a robust **"Single Hull of Summed Points"** strategy to ensure manifold integrity across mixed dimensionality.

### 6.1 The "Single Hull" Strategy
Unlike fragmented boolean unions, `jot/grow` calculates the Minkowski sum for each convex component (primitive) by:
1. Collecting all vertices of the subject primitive ($V_s$) and the tool ($V_t$).
2. Computing the combined point cloud $V_{sum} = \{ v_s + v_t \mid v_s \in V_s, v_t \in V_t \}$.
3. Generating a single **3D Convex Hull** of the combined set.

### 6.2 Dimensional Integrity
The operator automatically adapts to the dimensionality of the result:
- **3D Solid**: If the summed points form a volume, the result is tagged as `type: "closed"` and returned as a manifold 3D mesh.
- **2D Surface**: If all summed points are coplanar (e.g., 2D subject + 2D tool), the operator extracts the faces of the hull and returns it as a formal `type: "surface"`, avoiding "pancake" instabilities in the 3D unioner.

### 6.3 Mixed-Dimensionality Support
- **Solid Subject + Flat Tool**: Correctly produces a 3D solid (e.g., a $12 \times 12 \times 10$ box from a $10 \times 10 \times 10$ cube grown by a $2 \times 2$ square).
- **Flat Subject + Solid Tool**: Correctly promotes the result to a 3D solid.
