# Spatial Query & Rigid Alignment

JotCAD uses a high-level "Socket" metaphor for alignment. This system is designed to be resilient to microscopic geometric fluctuations and intuitive for rapid "Blackboard" sketching.

## 1. The Rigid Matrix Mandate (Invariant)

To ensure mathematical stability during complex multi-part alignments, JotCAD enforces a strict separation of transformation types:

1.  **Placement (`Shape.tf`)**: The transformation matrix is strictly **Orthonormal**. It carries only **Translation** and **Rotation**. This ensures that inverting a frame (the `by()` operation) is numerically stable and never distorts space.
2.  **Deformation (Geometry)**: All non-rigid transforms—**Scale**, **Mirror**, and **Skew**—are **"Baked"** directly into the geometry vertices.
    -   *Implementation*: When a user calls `.scale(2)`, the C++ kernel generates a *new* Content-ID (CID) with scaled vertex coordinates and resets the local `tf` to identity.

## 2. Functional Selection

Instead of string-path guessing, JotCAD uses explicit measurement and ranking to target features.

### 2.1 Bucketed Extremities (Resilience)
The `highest()` and `lowest()` operators group features into **Clusters** (Buckets) based on their measurements. Features within a default epsilon (`1e-4`) are treated as being in the same bucket, allowing selection of "all bolts on a flange" even with microscopic numerical noise.

### 2.2 Measurement Suite
Features are targeted by their physical role using measurement operators:
- **`z()`**, **`y()`**, **`x()`**: Spatial span (Intervals).
- **`facing(vector)`**: Alignment with a world direction.
- **`area()`**, **`length()`**, **`volume()`**: Geometric size.

### 2.3 Selection Examples

| Intent | Selection String |
| :--- | :--- |
| **Highest points** | `highest(z(), 0)` |
| **Largest face** | `highest(area(), 0)` |
| **Top-facing face** | `highest(facing(UP), 0)` |
| **Sideways faces** | `highest(facing(UP), 1)` |

## 3. The Alignment Vocabulary

Alignment is the composition of three active operators: **`at`**, **`by`**, and **`to`**.

### 3.1 `at(target, [recipe])` — Scoped Context
- **`target`**: A shape or group (usually produced by `faces()`, `corners()`, or `rank()`).
- **Behavior**: Temporarily reorients the **entire subject** so that the target feature is at the origin $(0,0,0)$ facing $+Z$. It applies the `recipe` in this local frame and then projects the result back to world space.

### 3.2 `by(reference)` — Relative Composition
- **Behavior**: Composes the current transform with the reference's transform ($T_{new} = T_{ref} \times T_{old}$).
- **Usage**: This is the fundamental movement operator. `move(v)` is sugar for `by(Translation(v))`.

### 3.3 `to(destination)` — Absolute Placement
- **Behavior**: Moves the shape so its **Birth Origin** matches the destination frame exactly ($T_{new} = T_{dest}$).
- **Identity**: `a.to(b)` is strictly equivalent to `a.o().by(b)`.

## 4. Blackboard Workflow Examples

### Bolting a Bracket to a Wall (Grab & Place)
"Grab the bracket by its bottom-most face and place it on the wall's front surface."
```js
h = Bracket.faces().lowest(z(), 0) // Grab handle (bottom face)
s = Wall.faces().highest(z(), 0)   // Socket (top face)
Bracket.by(h.o()).by(s)
```

### Resetting to a Known Point
"Place the part's original birth origin at the world coordinate [10, 10, 0]."
```js
Part.to(O().move(10, 10, 0))
```

### Stacking Parts (The Sandwich)
```js
A.group(
    B.by(B.faces().lowest(z(), 0).o()).by(A.faces().highest(z(), 0)), // B's bottom at A's top
    C.by(C.faces().lowest(z(), 0).o()).by(B.faces().highest(z(), 0))  // C's bottom at B's top
)
```

## 5. Multi-Point Solving

When a selection returns multiple points (e.g., `at(highest(z(), 0))` for all top-face bolts), the `to()` and `by()` operators automatically perform a **Rigid SVD Fit**:
1.  **Translation**: Matches the centroids of the point sets.
2.  **Rotation**: Minimizes the angular error between the vectors of the subject and target.
3.  **Result**: A rigid move with no distortion, providing the "Best Mechanical Fit" for noisy geometry.
