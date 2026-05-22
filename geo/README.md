# Geometry Domain (geo)

This directory is responsible for the JotCAD geometry engine, including its VFS integration, geometric algorithms, and rendering pipeline.

## Sub-directories

- **[ops/](./ops)**: The JotCAD Language interface. Contains VFS Operators that map DSL calls to geometric kernels.
- **[core/](./core)**: VFS/C++ Integration. Handles port injection, typed execution, and registration.
- **[data/](./data)**: Fundamental Data Models. Defines `Shape` and `Geometry` identities.
- **[algorithms/](./algorithms)**: Pure Geometric Algorithms. Contains CGAL-based kernels for creating and modifying shapes.
- **[math/](./math)**: Spatial Logic. Matrices, coordinate systems, and geometric predicates.
- **[render/](./render)**: Visualization. Robust triangulation and pixel rasterization.
- **[infra/](./infra)**: Build infrastructure and shared library glue code.
- **[test/](./test)**: Verification suite for the geometry domain.
- **[boolean/](./boolean)**: 3D CSG (Union, Cut, Clip) logic via CGAL.
- **[pack/](./pack)**: Geometric binning and nesting solver for 2D sheet optimization.

## Recent Improvements

- **New Operators**: Added `dup` (efficient duplication) and `gap` (padding) operators.
- **Robust Triangulation**: Implemented Newell's Method for polygon normal calculation in the triangulation engine. This ensures consistent lighting and correct domain identification for non-convex islands and split remainder geometries.
- **Sheet Packing**: Enhanced the `pack` operator with support for arbitrary sheet shapes (Exterior Subtraction IFP) and multiple orientations (rotations).
