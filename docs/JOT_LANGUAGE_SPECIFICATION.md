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

### 1.4. Logical Pass-Throughs (Optimized Aliases)

Some operations (like `.pdf()` or `.stl()`) perform side-effects but should not
"pollute" the functional chain of geometric selectors.

- **Alias Assertion:** These operations use `metadata.aliases` to declare that
  their functional result (`$out`) is logically identical to their input
  (`$in`).
- **Optimization Mode (`optimizeAliases: true`):**
  - The compiler identifies these "Tee" operations and returns the primary subject while recording the side-effect.
  - `Box(10).pdf("out.pdf").offset(2)` resolves to an `offset` selector pointing directly to `Box`.

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

## 6. Spatial Alignment (`.origin()`)

Every object maintains its **Birth Orientation**. The `.origin()` operator uses
the inverted transformation matrix to return the shape to its birth frame at
(0,0,0).

## 7. Comprehensive Operation Reference

(Standard operators: Arc, Box, Orb, Tri, Part, rotate, move, size, etc.)

## 8. VFS Providers & Parametric Symbols

### 8.1. Late-Bound Symbols

VFS request parameters are bound as symbols within the evaluation context.

- **Syntax:** `Box(10, 10, length)`
- **Resolution:** `length` is resolved from the VFS `parameters` object during evaluation.

### 8.2. Deterministic Identity (CID)

The Mesh-VFS calculates the Content-ID based on the **Canonical Selector**, ensuring that logically equivalent requests share the same address.
