# JotCAD Language Specification (Next Gen)

## 1. Core Philosophy

The JotCAD language is a **Functional Request DSL**. It provides a high-level,
human-readable syntax for describing geometry, while remaining 100% decoupled
from the actual computation.

- **Standalone Grammar:** JotCAD uses a subset of JavaScript's syntax
  (expressions, chaining, object literals) but is a standalone, non-executable
  grammar.
- **Parsed, Not Executed:** Expressions are parsed into an Abstract Syntax Tree
  (AST) that maps to a VFS Selector chain. There is no runtime environment,
  avoiding arbitrary code execution risks.
- **Symbols as Literals:** Unquoted identifiers (e.g., `length` in
  `Box(length)`) are treated as native **Symbol Literals**, acting as late-bound
  placeholders for VFS parameters.
- **Expressions as Addresses:** Every line of JotCAD code resolves to a
  deterministic **Mesh-VFS Selector**.
- **Silent Construction:** Writing an expression does not execute work. It only
  builds the "recipe".
- **Lexical Pithiness:** Prefer short, whole words for operations. Avoid cryptic
  abbreviations.
- **Diameter Standard:** Standardize on **Diameter** (Width/Bounding Envelope)
  for all primitives. Diameter is a universal property of every shape and aligns
  with physical measurement tools.
  - **Normalization:** The engine automatically converts `radius` ($d=2r$) and
    `apothem` (for hexagons) to `diameter` to ensure deterministic Content-IDs
    across the mesh.
- **Angular Turns:** Use **Turns** (Tau) where `1.0` is a full rotation
  ($360^\circ$).
- **Demand-Driven:** Work is only triggered when a requester performs a `READ`.

### 1.1 Naming Conventions (Casing)

The JotCAD DSL distinguishes between object creation and geometric
transformation through casing. **Lookups are strictly case-sensitive**:

- **Constructors (PascalCase):** Used for creating new shapes.
  - Examples: `Box(10, 20)`, `Orb(5)`, `Group(a, b)`.
  - If called as `box(10)`, the compiler will not resolve it to the `shape/box`
    constructor unless explicitly registered with that casing.
- **Operations (camelCase):** Used for methods applied to existing subjects.
  - Examples: `shape.offset(2)`, `shape.rotateX(0.5)`, `shape.extrude(10)`.
- **Aliases:** Short-form operations also follow camelCase (e.g., `.rx()`,
  `.sz()`).

### 1.2. Absence of Control Flow

The JotCAD DSL intentionally excludes traditional programming control flow
structures.

- **No Iteration:** There are no `for` or `while` loops. Iteration is handled
  implicitly by the **Universal Sequence Principle**. Utility operations like
  `range()` or `iota()` are unrolled into sequences by the compiler.
- **No Recursion:** Functions cannot call themselves. The language builds a
  static, deterministic tree of operations.
- **No Conditionals:** There are no `if/else` or `switch` expressions. Logic is
  applied via **Selectors** and **Implicit Mapping** over data sets.
- **Host-Logic Separation:** Any complex procedural logic (loops, branches,
  recursion) should be executed in the host environment (e.g., JavaScript) to
  generate the static JotCAD expression.

### 1.3. Logical Pass-Throughs (Optimized Aliases)

Some operations (like `.pdf()` or `.stl()`) perform side-effects but should not
"pollute" the functional chain of geometric selectors.

- **Alias Assertion:** These operations use `metadata.aliases` to declare that
  their functional result (`$out`) is logically identical to their input
  (`$in`).
- **Optimization Mode (`optimizeAliases: true`):**
  - The compiler identifies these "Tee" operations and moves them to a separate
    **Side Demands** collection.
  - The expression `Box(10).pdf("out.pdf").offset(2)` resolves to a sequence:
    `[Box(10).offset(2), Box(10).pdf("out.pdf")]`.
  - **Benefit:** The `offset` selector remains clean and cacheable, as it points
    directly to the `Box` instead of a PDF wrapper.
- **Unoptimized Mode (`optimizeAliases: false`):**
  - The expression resolves to a single nested VFS selector:
    `op/offset?in=op/pdf?in=shape/box`.
  - Work is still performed via VFS link-following, but the selector identity is
    tied to the side-effect.
- **Deduplication:** Side demands are automatically de-duplicated by their
  canonical VFS selector to avoid redundant work during complex mapping
  operations.

## 2. The Universal Sequence Principle

In JotCAD, every object is treated as an ordered **Sequence**. This enables
powerful set-based operations without explicit loops.

### 2.1. Implicit Context Propagation

The JotCAD compiler automatically threads the "Active Subject" through nested
expressions. This enables extremely concise parametric definitions where the
coordinate system or parent geometry is inherited implicitly.

- **Root Constructor:** `Box()` at the top level has no implicit input.
- **Method Call:** In `a.Box()`, the subject `a` is automatically passed as the
  implicit input to `Box`.
- **Nested Context:** In `a.b(Box())`, the subject `a` is propagated into the
  arguments of `b`. Thus, `Box()` is evaluated with `a` as its active context.
- **Explicit Override:** If an operation is provided with an explicit input
  (e.g., `a.b(Box({ $in: c }))`), the implicit context is ignored for that
  input slot.

### 2.2. Implicit Mapping

Any operation applied to a sequence is automatically mapped over its individual
members.

- `Group(Box(10), Arc(10)).extrude(5)` -> Returns a sequence containing one 3D
  box and one 3D cylinder.

### 2.2. Cartesian Product Generation

If an operation is called with a sequence of arguments, it produces the
**Cartesian Product** of the subject and those arguments. This is the primary
mechanism for generating arrays and patterns.

- `Box(10).rotateX(0.1, 0.2)` -> Returns a sequence of two boxes, one at 36° and
  one at 72°.
- `Arc(10, 20).rotateX(0.5)` -> Returns a sequence of two arcs, both rotated
  180°.

### 2.3. Universal Flattening

The language engine automatically flattens nested arrays encountered in the
argument stack. This ensures that complex nested structures (like those
generated by parametric scripts) are merged into a clean, flat sequence flow.

- `Group([a, b], [c, d], e)` -> Consumed as a single sequence of
  `(a, b, c, d, e)`.

## 3. Sequential Argument Harvesting

Operations use a **Greedy Harvesting** model to pull and transform values from
the argument stack.

### 3.1. Singleton Consumers

- **`num`**: First numeric value.
- **`str`**: First string value.
- **`bool`**: First boolean value.
- **`jot`**: First VFS Selector or nested expression.
- **`vec3`**: First array of 3 numbers `[x, y, z]`.
- **`intv`**: Normalizes ranges (e.g., `10` -> `[-5, 5]`).

### 3.2. Greedy Consumers

These consumers perform **Universal Flattening** on the stack.

- **`nums`**: Collects all numbers. Expands range objects `{ge, le, by}`.
- **`jots`**: Collects all VFS Selectors and nested expression sequences.
- **`tags(whitelist)`**: Collects matching strings into a boolean map.

## 4. The Workbench Principle (Alignment)

To enable "Blind Coupling" of disparate shapes, the language enforces a standard
**Fundamental Alignment** for all geometric features.

### 4.1. The Workbench Metaphor

Imagine a machinist's workbench with a reference corner at (0,0,0). When any
feature (a corner, an edge, a face) is selected as an anchor, the system
effectively "places it on the workbench" in a normalized orientation before
applying operations.

### 4.2. Standard Axial Mapping

All logical anchors (`corners`, `edges`, `faces`) are normalized to this frame:

- **Origin (0,0,0):** The geometric center or start-point of the feature.
- **X-Axis:** The **Primary Direction**. (Along the length of an edge, or along
  the bisector of a corner).
- **Y-Axis:** The **Secondary Direction**. (Tangent to the path or surface).
- **Z-Axis:** The **Normal Direction**. (Pointing "Up" or "Out" from the
  material).

## 5. Identity and Attributes

The language provides a symmetrical system for managing geometric components and
their associated data.

### 5.1. Components (Geometric Identity)

- **`Part(name, geometry)`**: Constructor that wraps geometry in a named
  semantic container.
- **`.as(name)`**: Shorthand operator to label the subject as a named part.
- **`.part(name)`**: Selector operator that retrieves a sub-component by its
  name.

### 5.2. Metadata (Data Attributes)

- **`.set(key, val)`**: Stores an arbitrary primitive value (number, string) in
  the shape's tag dictionary.
- **`.get(key, [type])`**: Terminal operator that retrieves a local tag value.

## 6. Interaction Surrogates (Gaps)

These operators define how visible parts and their invisible clearances
(surrogate tools) interact during boolean operations.

- **`.gap()`**: **Transparent Cutter.** Marks the subject as "cutting space." It
  carves volume out of other parts in booleans but is invisible in the final
  output.
- **`.gapOf(part)`**: **Enclosing Surrogate.** Marks the subject as the
  clearance volume for the provided `part`.
- **`.gapBy(volume)`**: **Self-Enclosing Surrogate.** Marks the provided
  `volume` as the clearance for the subject.

## 7. Spatial Alignment (`.origin()`)

Every object maintains its **Birth Orientation**. The `.origin()` operator uses
the inverted transformation matrix to return the shape to its birth frame at
(0,0,0).

### 7.1. The Anchor Pattern (`.at()`)

The `.at(anchor, op)` operator allows for localized work relative to a specific
sub-feature or local origin:

1. **At Origin:** Inverts the `anchor`'s matrix to reach its local coordinate
   system.
2. **Apply:** Applies the operation `op` at that local origin.
3. **Project:** Re-applies the `anchor`'s original matrix to the result.

## 8. Comprehensive Operation Reference

### 8.1. Constructors (Fundamental Shapes)

| Op                | Arguments          | VFS Path         | Description                     |
| :---------------- | :----------------- | :--------------- | :------------------------------ |
| `Arc()`           | `diameter, [opts]` | `shape/arc`      | 2D Circle or Arc.               |
| `Box()`           | `w, h, [d]`        | `shape/box`      | 2D Rectangle or 3D Box.         |
| `Orb()`           | `diameter, [opts]` | `shape/orb`      | 3D Sphere.                      |
| `Tri()`           | (Polymorphic)      | `shape/triangle` | Various triangle forms.         |
| `Part()`          | `name, jot`        | `op/tag`         | Named semantic container.       |
| `Pt()`            | `x, y, [z]`        | `shape/point`    | A single vertex coordinate.     |
| `Origin()`        | -                  | `shape/origin`   | Coordinate [0,0,0].             |
| `X() / Y() / Z()` | `[num]`            | `shape/axis`     | Points or Planes along an axis. |

### 8.2. Geometric Operators (Selection & Topology)

| Op           | Arguments    | VFS Path     | Description                      |
| :----------- | :----------- | :----------- | :------------------------------- |
| `.extrude()` | `height`     | `op/extrude` | 2D to 3D projection.             |
| `.offset()`  | `radius`     | `op/offset`  | Boundary expansion/contraction.  |
| `.outline()` | -            | `op/outline` | Extract boundary segments.       |
| `.points()`  | -            | `op/points`  | Extract all vertices (0D).       |
| `.corners()` | -            | `op/corners` | Extract oriented corner anchors. |
| `.edges()`   | -            | `op/edges`   | Extract oriented edge anchors.   |
| `.loop()`    | -            | `op/loop`    | Close point cloud into path.     |
| `.fill()`    | -            | `op/fill`    | Surface generation from path.    |
| `.nth()`     | `...indices` | `op/nth`     | Filter collection by index.      |

### 8.3. Boolean & Assembly Logic

| Op            | Arguments | VFS Path      | Description                         |
| :------------ | :-------- | :------------ | :---------------------------------- |
| `.and()`      | `...jots` | `op/group`    | Union (Addition).                   |
| `.cut()`      | `...jots` | `op/cut`      | Difference (Subtraction).           |
| `.cutFrom()`  | `target`  | `op/cut`      | Relative Difference.                |
| `.clip()`     | `...jots` | `op/clip`     | Intersection.                       |
| `.disjoint()` | -         | `op/disjoint` | Collision-Resolution (No overlaps). |
| `.gap()`      | -         | `op/tag`      | Set as Transparent Cutter.          |
| `.gapOf()`    | `part`    | `op/tag`      | Set as surrogate for part.          |
| `.gapBy()`    | `volume`  | `op/tag`      | Set surrogate for subject.          |

### 8.4. Transformations & Styling

| Primary Op   | Alias   | Arguments  | Description               |
| :----------- | :------ | :--------- | :------------------------ |
| `.rotateX()` | `.rx()` | `...turns` | Rotation (Tau-based).     |
| `.rotateY()` | `.ry()` | `...turns` | Rotation (Tau-based).     |
| `.rotateZ()` | `.rz()` | `...turns` | Rotation (Tau-based).     |
| `.move()`    | `.at()` | `x, y, z`  | Absolute Translation.     |
| `.size()`    | `.sz()` | `factor`   | Scaling.                  |
| `.color()`   | -       | `str`      | Semantic color tag.       |
| `.as()`      | -       | `str`      | Assign part name.         |
| `.set()`     | -       | `key, val` | Store arbitrary metadata. |
| `.origin()`  | -       | -          | Return to birth frame.    |

### 8.5. Terminal Operations (Exports)

| Op       | Arguments      | VFS Path | Description              |
| :------- | :------------- | :------- | :----------------------- |
| `.pdf()` | `name, [opts]` | `op/pdf` | Export to PDF.           |
| `.stl()` | `name`         | `op/stl` | Export to STL.           |
| `.png()` | `name, [opts]` | `op/png` | Render snapshot.         |
| `.bom()` | -              | `op/bom` | Aggregated Part List.    |
| `.get()` | `key, [type]`  | `op/get` | Retrieve metadata value. |

## 9. Tolerance-Driven Resolution (`zag`)

JotCAD uses the **`zag`** algorithm to determine geometric resolution based on
physical tolerance.

- **The Goal:** Ensure edges never deviate from the ideal curve by more than a
  specified `tolerance`.
- **Calculation:** `zag(diameter, tolerance)` calculates the required number of
  sides.

## 10. VFS Providers & Parametric Symbols

The language serves as the definition layer for dynamic, parametric VFS
providers.

### 10.1. Late-Bound Symbols

VFS request parameters are bound as symbols within the evaluation context. This
enables the creation of parametric "recipes" that remain abstract until a `READ`
is performed.

- **Syntax:** `Box(10, 10, length)`
- **Resolution:** When requested via `{ "path": "shape/my_box", "parameters": { "length": 50 } }`, the symbol
  `length` is automatically resolved to the value `50` during evaluation.

### 10.2. Semantic Canonicalization (`.spec()`)

To ensure **Deterministic Identity (CID)** across the mesh, providers must
canonicalize diverse input parameters into a single, standard form. The
`.spec()` operator defines this contract.

- **Aliases:** Maps multiple external keys to a single internal symbol (e.g.,
  `L` and `len` both map to `length`).
- **Defaults:** Provides fallback values for missing parameters.
- **Example:**
  ```javascript
  Box(length, width).spec({
    length: { alias: ['L', 'len'], default: 50 },
    width: { alias: ['W', 'w'], default: 50 },
  });
  ```
- **Deterministic CID:** The Mesh-VFS calculates the Content-ID based on the
  **Canonical Selector** (the result of the `.spec()` transformation), ensuring
  that `?L=50` and `?length=50` resolve to the same data artifact.
