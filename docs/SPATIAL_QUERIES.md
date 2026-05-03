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

### 3.1 `at(query)` — The Query (Reference)
Called without a recipe, `at()` returns the oriented **Frame** (coordinate system) of the queried feature.
- **Congruent**: The Z-axis points **out** of the part (Standard orientation).

### 3.2 `to(query)` — The Socket (Mate)
Similar to `at()`, but returns an **Inverted Frame**.
- **Opposed**: The Z-axis points **into** the part.
- **Usage**: Used for **Surface-to-Surface Mating**.

### 3.3 `by(reference)` — The "Grab"
Moves the entire shape so that the specified `reference` frame is now the **World Origin** $(0,0,0)$ facing $+Z$.
- **Math**: $T_{new} = \text{inverse}(T_{ref}) \times T_{old}$

### 3.4 `to(destination)` — The "Place"
Moves the entire shape so that its current origin $(0,0,0)$ matches the `destination` frame.
- **Math**: $T_{new} = T_{dest} \times T_{old}$

### 3.5 `place(template_shape)` — The "Distributor"
Instantiates a template shape at every frame in the current collection. This is a constructive operator typically used with selection generators.
- **Usage**: `eachCorner().place(Bolt())`
- **Result**: A group containing transformed copies of the template.

## 4. Blackboard Workflow Examples

### Bolting a Bracket to a Wall (Mating)
"Grab the bracket by its back face and place it on the wall's front surface."
```js
Bracket.by(Bracket.at('z-')).to(Wall.to('z+'))
```

### Inserting a Bearing (Insertion)
"Grab the bearing by its center and place it at the housing's coordinate point."
```js
Bearing.by(Bearing.at('origin')).to(Housing.at('socket'))
```

### Flush Alignment with a Sheet
"Grab the part by its left edge and place it on the sheet's left boundary."
```js
Part.by(Part.at('x-')).to(Sheet.at('x-'))
```

### Stacking Parts (The Sandwich)
```js
A.group(
    B.by(B.at('z-')).to(A.to('z+')), // B sits on A
    C.by(C.at('z-')).to(B.to('z+'))  // C sits on B
)
```

## 5. Multi-Point Solving

When a query returns multiple points (e.g., `at('? z+')` for all top-face holes), the `to()` and `by()` operators automatically perform a **Rigid SVD Fit**:
1.  **Translation**: Matches the centroids of the point sets.
2.  **Rotation**: Minimizes the angular error between the vectors of the subject and target.
3.  **Result**: A rigid move with no distortion, providing the "Best Mechanical Fit" for noisy geometry.
