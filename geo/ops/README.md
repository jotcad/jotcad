# Geometry Operators (geo/ops)

This directory contains the JotCAD DSL interface. Each file implements a `P::execute` method that maps a VFS Selector to a specific geometric algorithm.

- **Atomic Responsibility**: Map VFS parameters to typed kernel calls.
- **Dependencies**: Depends on `core/` for the processor and `algorithms/` for the kernels.
