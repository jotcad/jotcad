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

- **Definition Store:** Definitions are stored in a persistent, mutable "Project Store" outside the VFS (e.g., `localStorage` or a `.jot/` directory). This is the user's editable "Source of Truth."
- **VFS Fulfillment:** When a dynamic operation is executed, the *result* is cached in the VFS using the standard **Pure Flat CID** identity rule: `CID = hash(JCB(path, parameters))`.

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

## 5. UX Workflow

1. **Argument Definition:** The user explicitly defines names and types (e.g., `radius: number`).
2. **Bound Variables:** These names are automatically injected as identifiers into the Jot expression editor.
3. **Sandbox Evaluation:** An "Evaluate" button allows the user to test the logic using the current "Test Values" defined in the UI.
4. **Global Availability:** Once defined, the operation can be called by name in any other `JotNode` or script across the mesh.
