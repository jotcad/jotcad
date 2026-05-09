# Spatial Query & Rigid Alignment

JotCAD uses a high-level "Socket" metaphor for alignment. This system is designed to be resilient to microscopic geometric fluctuations and intuitive for rapid "Blackboard" sketching.

## 1. The Rigid Matrix Mandate (Invariant)

To ensure mathematical stability during complex multi-part alignments, JotCAD enforces a strict separation of transformation types:

1.  **Placement (`Shape.tf`)**: The transformation matrix is strictly **Orthonormal**. It carries only **Translation** and **Rotation**. This ensures that inverting a frame (the `by()` operation) is numerically stable and never distorts space.
2.  **Deformation (Geometry)**: All non-rigid transforms—**Scale**, **Mirror**, and **Skew**—are **"Baked"** directly into the geometry vertices.
    -   *Implementation*: When a user calls `.scale(2)`, the C++ kernel generates a *new* Content-ID (CID) with scaled vertex coordinates and resets the local `tf` to identity.

## 2. Spatial Query Language (SQL)

Features are selected using compact, bucketed query strings.

### 2.1 Bucketed Extremities (Resilience)
Instead of exact coordinate sorting, SQL groups features into **Clusters** based on their absolute limits. Features within a default epsilon (`1e-4`) are treated as being at the "same height" or "same orientation."

### 2.2 Dominance Ranking (Priority)
Within a bucket, features are sorted by **Dominance** so that the "Main" face or longest edge is always at index `0`.
- **Faces**: Area-sorted (Largest first).
- **Edges**: Length-sorted (Longest first).

### 2.3 Syntax Table

| Symbol | Meaning | Example |
| :--- | :--- | :--- |
| `x, y, z` | **Position** | `z+` (Highest bucket), `z-` (Lowest) |
| `nx, ny, nz`| **Normal** | `nz+` (Up-facing), `nx0` (Side-facing) |
| `?` | **Hole** | Only internal parity faces. |
| `!` | **Shell** | Only outer boundary faces. |
| `_` | **Flat** | Only perfectly planar faces (skips curved). |
| `#tag` | **Tag** | Matches Zero-Base semantic tags. |
| `0, 1, ...` | **Bucket** | `z+1` (The second-highest cluster). |

## 3. The Alignment Vocabulary

Alignment is the composition of three active operators: **`at`**, **`by`**, and **`to`**.

### 3.1 `at(query, [recipe])` — Scoped Context
- **As an Operator**: Temporarily reorients the **entire subject** so that the queried feature is at the origin $(0,0,0)$ facing $+Z$. It applies the `recipe` in this local frame and then projects the result back to world space.
- **As a Query**: Returns the oriented **Frame** (coordinate system) of the queried feature.

### 3.2 `by(reference)` — Relative Composition
- **Behavior**: Composes the current transform with the reference's transform ($T_{new} = T_{ref} \times T_{old}$).
- **Usage**: This is the fundamental movement operator. `move(v)` is sugar for `by(Translation(v))`.

### 3.3 `to(destination)` — Absolute Placement
- **Behavior**: Moves the shape so its **Birth Origin** matches the destination frame exactly ($T_{new} = T_{dest}$).
- **Identity**: `a.to(b)` is strictly equivalent to `a.origin().by(b)`.

## 4. Blackboard Workflow Examples

### Bolting a Bracket to a Wall (Grab & Place)
"Grab the bracket by its back face and place it on the wall's front surface."
```js
h = Bracket.at('z-') // Grab handle
s = Wall.at('z+')    // Socket
Bracket.by(h.o()).by(s)
```

### Resetting to a Known Point
"Place the part's original birth origin at the world coordinate [10, 10, 0]."
```js
Part.to(o().move([10, 10, 0]))
```

### Stacking Parts (The Sandwich)
```js
A.group(
    B.by(B.at('z-').o()).by(A.at('z+')), // B's bottom at A's top
    C.by(C.at('z-').o()).by(B.at('z+'))  // C's bottom at B's top
)
```

## 5. Multi-Point Solving

When a query returns multiple points (e.g., `at('? z+')` for all top-face holes), the `to()` and `by()` operators automatically perform a **Rigid SVD Fit**:
1.  **Translation**: Matches the centroids of the point sets.
2.  **Rotation**: Minimizes the angular error between the vectors of the subject and target.
3.  **Result**: A rigid move with no distortion, providing the "Best Mechanical Fit" for noisy geometry.
