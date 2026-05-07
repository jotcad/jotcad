# Design Note: Efficient Coordinate Sequences (`jot:vec3s`)

## Problem Statement
The current `jot:shapes` consumer requires that every vertex in a `Link` or `Loop` be a formal VFS `Shape`. This leads to verbose JOT code or "CID bloat" where the VFS Selector for a simple path becomes an massive tree of nested Point Selectors.

## Proposed Solution
Implement a `jot:vec3s` consumer in the `JotCompiler` that harvests coordinates directly from the argument stack and promotes them to a structured list of vectors.

## Key Design Questions

### 1. Flattening Logic
The consumer must be robust against various user input styles:
- **Nested**: `[[0,0], [10,10]]`
- **Contiguous**: `[0,0], [10,10]`
- **Mixed**: `[0,0], Point(10,10), [20,0]`

### 2. Group Extraction (The "Dive")
If a `Group` shape is passed to a `vec3s` consumer, should the compiler automatically extract the `translation` component of every child?
- **Pro**: `Group(a, b, c).Link()` becomes a very powerful way to connect objects.
- **Con**: Might be "too much magic" if the user intended to link the groups as whole entities.

### 3. VFS Compactness vs. Cache Granularity
- **Current**: `Link(Point(1,1), Point(2,2))` -> Selector contains two CIDs. Changing one point reuses the other.
- **Proposed**: `Link([1,1], [2,2])` -> Selector contains literal numbers. Changing one point changes the hash of the whole Link.
- **Decision**: For paths (which are often high-vertex), the compactness of literal numbers is likely superior to the overhead of thousands of Point CIDs.

### 4. Semantic Ambiguity
A list like `[10, 20]` could be a `vec2` point OR a `size` interval.
- **Rule**: Consumers are "Greedy and Typed". If an operator's schema asks for `jot:vec3s`, it claims all following arrays as coordinates until it hits a non-array or another typed argument.

## Implementation Plan
1. Update `jot/src/compiler.js` to include `JotVec3sConsumer`.
2. Update `geo/ops/path_op.h` to use `jot:vec3s` in the schema.
3. Verify that `Link([0,0], [10,0])` works without manual `Point()` wrapping.
