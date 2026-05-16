# JotCAD Selection Example: The Corner Hole Pattern

This example demonstrates the **Scoped Context** (`at`) model, showing how functional selection chains decompose a shape into local coordinate systems for precise, topology-resilient modifications.

## The Problem
**Cut a mounting hole 10mm away from the top-left corner of every vertical face of a hexagonal prism.**

```js
// 1. Define the subject
Prism = Hexagon(d=100).extrude(50);

// 2. Define the selection logic
// We want faces pointing sideways (facing Z is 0)
// lowest(facing(Z()), 0) targets the bucket with the smallest dot product (0).
vertical_faces = Prism.faces().lowest(facing(Z()), 0);

// 3. Perform the scoped modification
Prism.at(vertical_faces, 
    at(corners().lowest(x(), 0).highest(y(), 0), 
        cut(Cylinder(d=5, h=10).move(10, -10))
    )
);
```

## How it Works

### 1. Sequential Subject Reduction (`at`)
The `at` operator is a **Scoped Reorientation** tool. It doesn't just find a point; it moves the entire "Workbench" to that point.
- **`at(vertical_faces, ...)`**: Finds all faces with a normal perpendicular to Z. For each face, it temporarily reorients the Prism so the face center is $(0,0,0)$ and the face normal is $Z+$.
- **Nested `at(...)`**: Because the first `at` changed the coordinate system, the second `at` works in a **Local Frame** where the face is the XY plane.

### 2. Functional Ranking
Instead of "guessing" string paths, we use explicit measurements:
- **`lowest(x(), 0)`**: Measures the X-coordinate of each corner and selects the lowest bucket (the "Left" extremity).
- **`highest(y(), 0)`**: Measures the Y-coordinate and selects the highest bucket (the "Top" extremity).
- **Result**: Combined, they select the **Top-Left Corner** of the current face context.

### 3. Local Bounding Boxes
Selection always respects the **Local Bounding Box** of the current frame:
- In the face context, the "Left" side of the Prism is simply the $X_{min}$ of the face itself. 
- This ensures the hole is placed relative to the **actual boundaries** of that specific face, regardless of whether the prism is a Box, a Hexagon, or a complex custom shape.

### 4. Projection
Once the `cut` is performed in the nested corner context, the compiler automatically projects the modification back through the transform stack into the world-space prism.
