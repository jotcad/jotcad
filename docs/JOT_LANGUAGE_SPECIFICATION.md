# JotCAD Language Specification (Next Gen)

## 1. Core Philosophy
The JotCAD language is a **Functional Request DSL**. It provides a high-level, human-readable syntax for describing geometry, while remaining 100% decoupled from the actual computation.

- **Expressions as Addresses:** Every line of JotCAD code resolves to a deterministic **Mesh-VFS Selector**.
- **Lexical Pithiness:** Prefer short, whole words for operations. Avoid cryptic abbreviations.
- **Diameter Standard:** Standardize on **Diameter** (Width/Bounding Envelope) for all primitives. Diameter is a universal property of every shape and aligns with physical measurement tools.
- **Angular Turns:** Use **Turns** (Tau) where `1.0` is a full rotation ($360^\circ$).
- **Demand-Driven:** Work is only triggered when a requester performs a `READ`.

## 2. The Workbench Principle (Alignment)
To enable "Blind Coupling" of disparate shapes, the language enforces a standard **Fundamental Alignment** for all geometric features.

### 2.1. The Workbench Metaphor
Imagine a machinist's workbench with a reference corner at (0,0,0). When any feature (a corner, an edge, a face) is selected as an anchor, the system effectively "places it on the workbench" in a normalized orientation before applying operations.

### 2.2. Standard Axial Mapping
All logical anchors (`corners`, `edges`, `faces`) are normalized to this frame:
- **Origin (0,0,0):** The geometric center or start-point of the feature.
- **X-Axis:** The **Primary Direction**. (Along the length of an edge, or along the bisector of a corner).
- **Y-Axis:** The **Secondary Direction**. (Tangent to the path or surface).
- **Z-Axis:** The **Normal Direction**. (Pointing "Up" or "Out" from the material).

## 3. Sequential Argument Harvesting
Operations use a **Greedy Harvesting** model to pull and transform values from the argument stack.

### 3.1. Singleton Consumers
- **`num`**: First numeric value.
- **`str`**: First string value.
- **`bool`**: First boolean value.
- **`jot`**: First VFS Selector or nested expression.
- **`vec3`**: First array of 3 numbers `[x, y, z]`.
- **`intv`**: Normalizes ranges (e.g., `10` -> `[-5, 5]`).

### 3.2. Greedy Consumers
- **`nums`**: Collects all numbers. Expands range objects `{ge, le, by}`.
- **`jots`**: Collects all VFS Selectors and nested arrays.
- **`tags(whitelist)`**: Collects matching strings into a boolean map.

## 4. Identity and Attributes
The language provides a symmetrical system for managing geometric components and their associated data.

### 4.1. Components (Geometric Identity)
- **`Part(name, geometry)`**: Constructor that wraps geometry in a named semantic container.
- **`.as(name)`**: Shorthand operator to label the subject as a named part.
- **`.part(name)`**: Selector operator that retrieves a sub-component by its name.

### 4.2. Metadata (Data Attributes)
- **`.set(key, val)`**: Stores an arbitrary primitive value (number, string) in the shape's tag dictionary.
- **`.get(key)`**: Terminal operator that retrieves a value from the tag dictionary. If called on an assembly, it may perform recursive aggregation (e.g., summing costs).

## 5. Interaction Surrogates (Gaps & Masks)
These operators provide advanced control over how parts interact during boolean operations (`disjoint`, `cut`, `and`).

- **`.gap()`**: **Transparent Cutter.** Marks the subject as "cutting space." It carves volume out of other parts in booleans but is invisible in the final output. Used for creating clearances and tolerances.
- **`.mask(shape)`**: **Boolean Surrogate.** The subject appears as itself visually, but uses the provided `shape` as its physical boundary during `cut` and `disjoint` operations. Used for performance (simplifying threaded holes) and manufacturing logic.

## 6. Spatial Reversion & The Anchor Pattern
Every object maintains its **Original Orientation**, accessible by inverting its accumulated matrix transforms.

### 6.1. Reversion (`.revert()`)
The `.revert()` operator inverts the subject's transformation matrix, effectively bringing it back to its "birth" state at the world origin (0,0,0).

### 6.2. The Anchor Pattern (`.at()`)
The `.at(anchor, op)` operator allows for localized work relative to a specific sub-feature or local origin:
1. **Revert:** Inverts the `anchor`'s matrix to reach its local coordinate system.
2. **Apply:** Applies the operation `op` at that local origin.
3. **Project:** Re-applies the `anchor`'s original matrix to the result.

## 7. Comprehensive Operation Reference

### 7.1. Constructors (Fundamental Shapes)
| Op | Arguments | VFS Path | Description |
| :--- | :--- | :--- | :--- |
| `arc()` | `diameter, [opts]` | `shape/arc` | 2D Circle or Arc. |
| `box()` | `w, h, [d]` | `shape/box` | 2D Rectangle or 3D Box. |
| `orb()` | `diameter, [opts]` | `shape/orb` | 3D Sphere. |
| `tri()` | (Polymorphic) | `shape/triangle` | Various triangle forms. |
| `Part()` | `name, jot` | `op/tag` | Named semantic container. |
| `pt()` | `x, y, [z]` | `shape/point` | A single vertex coordinate. |
| `origin()`| - | `shape/origin` | Coordinate [0,0,0]. |
| `X() / Y() / Z()`| `[num]` | `shape/axis` | Points or Planes along an axis. |

### 7.2. Geometric Operators (Selection & Topology)
| Op | Arguments | VFS Path | Description |
| :--- | :--- | :--- | :--- |
| `.extrude()` | `height` | `op/extrude` | 2D to 3D projection. |
| `.offset()` | `radius` | `op/offset` | Boundary expansion/contraction. |
| `.outline()` | - | `op/outline` | Extract boundary segments. |
| `.points()` | - | `op/points` | Extract all vertices (0D). |
| `.corners()` | - | `op/corners` | Extract oriented corner anchors. |
| `.edges()` | - | `op/edges` | Extract oriented edge anchors. |
| `.loop()` | - | `op/loop` | Close point cloud into path. |
| `.fill()` | - | `op/fill` | Surface generation from path. |
| `.nth()` | `...indices` | `op/nth` | Filter collection by index. |

### 7.3. Boolean & Assembly Logic
| Op | Arguments | VFS Path | Description |
| :--- | :--- | :--- | :--- |
| `.and()` | `...jots` | `op/group` | Union (Addition). |
| `.cut()` | `...jots` | `op/cut` | Difference (Subtraction). |
| `.cutFrom()`| `target` | `op/cut` | Relative Difference. |
| `.clip()` | `...jots` | `op/clip` | Intersection. |
| `.disjoint()`| - | `op/disjoint` | Collision-Resolution (No overlaps). |
| `.gap()` | - | `op/tag` | Set as Transparent Cutter. |
| `.mask()` | `shape` | `op/tag` | Set Boolean Surrogate. |

### 7.4. Transformations & Styling
| Primary Op | Alias | Arguments | Description |
| :--- | :--- | :--- | :--- |
| `.rotateX()` | `.rx()` | `...turns` | Rotation (Tau-based). |
| `.rotateY()` | `.ry()` | `...turns` | Rotation (Tau-based). |
| `.rotateZ()` | `.rz()` | `...turns` | Rotation (Tau-based). |
| `.move()` | `.at()` | `x, y, z` | Absolute Translation. |
| `.size()` | `.sz()` | `factor` | Scaling. |
| `.color()` | - | `str` | Semantic color tag. |
| `.as()` | - | `str` | Assign part name. |
| `.set()` | - | `key, val` | Store arbitrary metadata. |

### 7.5. Terminal Operations (Exports)
| Op | Arguments | VFS Path | Description |
| :--- | :--- | :--- | :--- |
| `.pdf()` | `name, [opts]` | `op/pdf` | Export to PDF. |
| `.stl()` | `name` | `op/stl` | Export to STL. |
| `.png()` | `name, [opts]` | `op/png` | Render snapshot. |
| `.bom()` | - | `op/bom` | Aggregated Part List. |
| `.get()` | `key` | `op/get` | Retrieve metadata value. |

## 8. Geometric Recipes

### 8.1. Threaded Bolt with Drill Mask
To keep a visual thread but cut a clean hole:
```javascript
const myBolt = Part('M10 Bolt', 
  Bolt(10, 50).mask(arc(10).extrude(50))
);

assembly.and(myBolt).disjoint();
```

### 8.2. Inset Corner Holes
```javascript
Hexagon(100).at(
  offset(-10).corners(), 
  arc(5).cut()
)
```

## 9. Tolerance-Driven Resolution (`zag`)
JotCAD uses the **`zag`** algorithm to determine geometric resolution based on physical tolerance.
- **The Goal:** Ensure edges never deviate from the ideal curve by more than a specified `tolerance`.
- **Calculation:** `zag(diameter, tolerance)` calculates the required number of sides.
