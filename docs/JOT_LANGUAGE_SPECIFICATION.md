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
- **Typed Symbols:** Unquoted identifiers (e.g., `length` in `Box(length)`) are
  treated as native **Symbol Literals**. To ensure VFS stability, symbols are
  **re-typed** during evaluation to match their target slots (e.g., `jot:number`).
- **Expressions as Addresses:** Every line of JotCAD code resolves to a
  deterministic **Mesh-VFS Selector**.
- **Silent Construction:** Writing an expression does not execute work. It only
  builds the "recipe".
- **Lexical Pithiness:** Prefer short, whole words for operations. Avoid cryptic
  abbreviations.
- **Diameter Standard:** Standardize on **Diameter** (Width/Bounding Envelope)
  for all primitives. Diameter is a universal property of every shape and aligns
  with physical measurement tools.
- **Interval Normalization:** Dimensional scalar values (like diameter) are
  automatically normalized into symmetric **Intervals** (e.g., `10` -> `[-5, 5]`)
  before reaching the VFS.
- **Standardized evaluate() Return:** `JotCompiler.evaluate()` strictly returns
  an **Array of Terminal Bundles** (`{ selector: Selector, schema: OutputPortSchema }[]`), 
  even for single expressions. This ensures consistent discovery of all 
  computational terminals and provides the associated schema metadata (e.g., type) 
  for each discovered port.
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

### 1.2. Type System

All types are normalized to the `jot:` prefix for internal lookup:

- **`jot:number`**: A single scalar value.
- **`jot:numbers`**: A list/sequence of scalar values.
- **`jot:shape`**: A single geometric entity (2D or 3D).
- **`jot:shapes`**: A list/sequence of geometric entities.
- **`jot:string`**: A UTF-8 string.
- **`jot:vec2` / `jot:vec3`**: Arrays of numbers.
- **`jot:interval`**: A pair of numbers `[min, max]`.

### 1.3. Absence of Control Flow

The JotCAD DSL intentionally excludes traditional programming control flow
structures.

- **No Iteration:** There are no `for` or `while` loops. Iteration is handled
  explicitly by the **Universal Sequence Principle**. Utility operations like
  `range()` or `iota()` return sequences (arrays) that are passed to operators.
- **No Recursion:** Functions cannot call themselves.
- **No Conditionals:** Logic is applied via **Selectors** and **Explicit Mapping** over data sets.

### 1.4. Strict Explicit Wiring (Assignment Model)

JotCAD avoids "magic" discovery of unconsumed values. Instead, the compiler
enforces a **Strict Assignment Model**. 

- **Explicit Targets:** Only values that are explicitly assigned to a variable 
  or wired to an output port using the arrow operator (`->`) are tracked by 
  the compiler.
- **Output Fulfillment:** A script is considered "successful" only if it 
  assigns values to every output port defined in the operator's schema.
- **Port Visibility:** Only values wired to formal schema ports (e.g., `$out`, 
  `debug`, `file`) are returned by the evaluation engine. Unassigned expressions
  are treated as dead code or trigger compiler errors in strict contexts.

### 1.5. Multi-Expression Support

The JOT parser supports scripts containing multiple expressions. However, 
in the context of a User Operator, each top-level statement MUST be an 
assignment to ensure deterministic output fulfillment.

```js
// Explicitly fulfilling multiple ports
Box(10) -> $out
Cylinder(5) -> debug

// Standard assignment
main = Box(20)
main.cut(Orb(5)) -> $out
```

## 2. The Universal Sequence Principle

In JotCAD, every object is treated as an ordered **Sequence**. This enables
powerful set-based operations without explicit loops.

### 2.1. Implicit Context Propagation (Subject Propagation)

The JotCAD compiler threads the "Active Subject" through method chains and deep
into argument evaluation.

- **Method Call:** In `a.b()`, the subject `a` is automatically passed as the
  implicit input to `b`.
- **Subject Propagation:** The active subject propagates into the evaluation of
  nested arguments. In `a.b(c())`, the nested `c()` call receives `a` as its
  ambient subject. This allows higher-order patterns like
  `Hexagon(30).at(eachCorner(), cut(Triangle(2)))` to work without special modes;
  `eachCorner()` and `cut()` both inherit the `Hexagon` subject to fulfill their
  input requirements.

### 2.2. Typed Consumer Mapping

The compiler utilizes **Schema-Driven Consumers** to enforce protocol integrity
between the JavaScript DSL and strict C++ typed kernels.

- **Consumer Responsibility:** Typed Consumers (e.g., `JotShapesConsumer`) are
  solely responsible for harvesting values from the argument pool or ambient
  subject and ensuring they match the required structure.
- **Plural Structure:** Consumers for plural types (`jot:shapes`, `jot:numbers`)
  guarantee the result is a JSON array. This satisfies C++ `std::vector`
  requirements regardless of whether a single item or multiple items were
  provided in the Jot code.
- **Deterministic Chaining:** To ensure stable method calls, operators should
  declare exactly one primary **affiliate** (usually `$in`). Non-affiliate
  arguments take priority in the argument pool, ensuring the ambient subject is
  reserved for the primary affiliate slot.

### 2.3. Sequence Handling

Operators that accept sequence types (e.g., `jot:numbers`, `jot:shapes`) are responsible for processing the entire array.

```jot
Box(10).rotateZ(range(3)) // range returns [0, 0.1, 0.2], rotateZ handles the array.
```

### 2.3. Universal Flattening

The language engine automatically flattens nested arrays encountered in the
argument stack. This ensures that complex nested structures are merged into a clean, flat sequence flow.

- `Group([a, b], [c, d], e)` -> Consumed as a single sequence of `(a, b, c, d, e)`.

### 2.4. Sequence Literals (Range Comprehension)

JotCAD provides a built-in **Sequence Literal** syntax for generating numeric
ranges. This syntax uses balanced square brackets `[]` and is visually
distinct from standard arrays.

- **Syntax:** `[ <start>? .. <end>? (by <step> | count <count>)? <inc>? ]`
- **Defaults:**
  - `start`: 0
  - `end`: 1 (Used if `count` is absent or if `count` is present but `by` is absent)
- **Behaviors:**
  - **`count N`**: Produces exactly $N$ items. If `by` is absent and `..end` is provided, the step is calculated to fit $N$ items into the range (Exclusive). If both are absent, step defaults to 1.
  - **`by S`**: Increments by $S$ until the limit is reached.
  - **`inc`**: Flag to include the endpoint (only applies to `by`-mode ranges).

#### Examples:

| Intent | JotCAD Syntax | Result |
| :--- | :--- | :--- |
| **Simple Integer Count** | `[count 5]` | `[0, 1, 2, 3, 4]` |
| **Linear progression** | `[count 8 by 5]` | `[0, 5, 10, ..., 35]` |
| **8-way circular array** | `[count 8 by 1/8]` | `[0, 0.125, 0.25, ..., 0.875]` |
| **Inclusive by Step** | `[0..100 by 20 inc]` | `[0, 20, 40, 60, 80, 100]` |
| **Descending Range** | `[10..0 by -1 inc]` | `[10, 9, ..., 0]` |
| **Exclusive Integer** | `[0..5]` | `[0, 1, 2, 3, 4]` (Default step 1) |

This notation integrates directly with the **Universal Sequence Principle**.
Chaining a method off a range (e.g., `[count 4].Box(10)`) automatically expands
the operation across the sequence.

## 3. Sequential Argument Harvesting

Operations use a **Greedy Harvesting** model to pull and transform values from
the argument stack based on the registered schema.

### 3.1. Singleton Consumers

- **`jot:number`**: First numeric value.
- **`jot:string`**: First string value.
- **`jot:bool`**: First boolean value.
- **`jot:shape`**: First VFS Selector (Shape).
- **`jot:op<...>`**: Generic Operation Signature for template validation.

### 3.2. Contiguous Greedy Consumers

These consumers perform **Universal Flattening** on the stack and harvest all 
matching values.

- **`jot:numbers`**: Contiguous numbers into an array.
- **`jot:shapes`**: Contiguous VFS Selectors (Shapes) into an array.

## 4. Higher-Order Provider Interface (C++)

C++ Operators utilize **Typed Port Injection** to maintain architectural purity.

- **Input Ports:** Labeled `$in` in schema; resolved via `vfs->read<json>` and injected as `const Shape&`.
- **Output Ports:** Labeled `$out` or custom sink names; injected as mutable references.

## 5. The Workbench Principle (Alignment)

All logical anchors (`corners`, `edges`, `faces`) are normalized to a standard workbench frame:

- **Origin (0,0,0):** The geometric center or start-point of the feature.
- **X-Axis:** The **Primary Direction**.
- **Y-Axis:** The **Secondary Direction**.
- **Z-Axis:** The **Normal Direction**.

### 5.1 `at(query, recipe)` — Scoped Reduction
The `at` operator implements **Sequential Subject Reduction**. It transforms the entire subject into the inverse frame of the queried feature, applies the `recipe`, and projects the result back to world space. 

```js
// Temporarily reorient to each corner to perform a local cut
Box(10).at(eachCorner(), cut(Cylinder(1)))
```

### 5.2 `by(ref)` vs `to(dest)` — Movement
- **`by(ref)`**: Relative Composition. Appends the reference's transform to the current one ($T_{new} = T_{ref} \times T_{old}$).
- **`to(dest)`**: Absolute Placement. Overwrites the current transform with the destination frame ($T_{new} = T_{dest}$). Equivalent to `.o().by(dest)`.

## 6. Spatial Alignment (`.o()`)

The `.o()` (or `.origin()`) operator is the primary tool for frame inversion. 
- **Method**: `X.o()` returns a shape with the inverse matrix of `X`.
- **Constructor**: `O()` or `Origin()` returns the identity frame.

This allows for **Feature Snapping**: `H.by(C.o())` drags the entire shape `H` so that its feature `C` lands at the world origin.

## 7. Functional Selection (`highest`/`lowest`)

Collections (Groups) can be filtered and sorted using the **Functional Measurement** model.

```js
// Select the largest child component
group.highest(area(), 0)

// Select the highest bucket of components
group.highest(z(), 0)
```

Measurement operators like `area()`, `z()`, and `facing()` are template operators applied to each component to produce a ranking value. Selection is performed using **Epsilon Bucketing**, which groups components with logically equivalent measurements. 

- **`highest`**: Sorts Descending (Highest value = Bucket 0).
- **`lowest`**: Sorts Ascending (Lowest value = Bucket 0).

## 8. Boolean Operations

Boolean operations are foundational for CSG (Constructive Solid Geometry) and
hierarchical assembly logic in JotCAD. All boolean operators use the
**Exact Rational Kernel** to ensure zero-drift, topologically perfect results.

### 8.1. Basic Operators

- **`cut(tools)`**: Subtraction. Removes the volume (or area) of the `tools`
  from the subject.
- **`join(tools)`**: Addition. Unions the subject and `tools`, preserving 
  assembly hierarchy but merging overlapping geometry.
- **`clip(tools)`**: Intersection. Keeps only the portion of the subject that
  is *inside* the `tools`.

### 8.2. Consolidating Fusion (`fuse`)

The `fuse(tools)` operator is a **Flattening Union**. Unlike `join`, which
preserves hierarchy (parent and child shapes), `fuse` merges all geometry into
a single, flat `Geometry` object.

- **3D Result**: Overlapping solids are merged into a single manifold shell; 
  interior faces are deleted.
- **2D Result**: Coplanar faces are merged into a single area.
- **Dimensionality**: Fusing a 3D box and a 2D rectangle results in a single 
  geometry containing both types of primitives.

### 8.3. Cross-Dimensional Logic

JotCAD's boolean engine is **Dimensionally Aware** and supports all combinations of 0D (Points), 1D (Segments), 2D (Surfaces), and 3D (Solids):

- **Solid Tool vs. Path**: Trims the path to the part outside the solid.
- **Solid Tool vs. Points**: Removes points that fall inside the solid volume.
- **Surface Tool vs. Path**: Splits the path at the point of intersection with the surface.
- **Segment Tool vs. Segment (Collinear)**:
    - **Cut**: Subtracts overlapping portions.
    - **Join**: Merges overlapping collinear segments into a single minimal segment (interval merging).
- **Point Tool vs. Segment**: Splits the segment into two parts at the point's location (if the point lies exactly on the segment).
- **Segment Tool vs. Point**: Removes points that lie exactly on the segment.
- **Coplanar Optimization**: If the subject and tool are both flat and lie on the same plane, the engine uses 2D PWH (Polygon With Holes) logic for maximum performance and exactness.

## 9. Blocks (Scoped Execution)

Blocks allow for scoped execution and non-destructive branching within a method 
chain.

### 9.1. Syntax and Passthrough

A block is defined using the `.{ ... }` syntax applied to a subject. 
The block **always returns its original subject**, acting as a "Tee" in the 
pipeline.

```js
// Box(10) is moved and scaled independently for 'debug', 
// but the main chain continues with the original Box(10).
Box(10).{
  move(5).scale(2) -> debug
}.cut(Orb(2)) -> $out
```

### 9.2. Subject Inheritance

Every top-level statement inside a block automatically inherits the block's 
subject as its ambient `$in`.

### 9.3. Lexical Scoping

Variables assigned inside a block are local to that block and do not persist 
in the parent scope.

```js
Box(10).{
  temp = move(5)
  temp.scale(2) -> debug
}
// 'temp' is no longer accessible here
```

## 10. Comprehensive Operation Reference

(Standard operators: Arc, Box, Orb, Tri, Part, rotate, move, size, etc.)

## 11. VFS Providers & Parametric Symbols

### 11.1. Late-Bound Symbols

VFS request parameters are bound as symbols within the evaluation context.

- **Syntax:** `Box(10, 10, length)`
- **Resolution:** `length` is resolved from the VFS `parameters` object during evaluation.

### 11.2. Deterministic Identity (CID)

The Mesh-VFS calculates the Content-ID based on the **Canonical Selector**, ensuring that logically equivalent requests share the same address.
