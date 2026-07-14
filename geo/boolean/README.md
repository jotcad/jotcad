# C++ Boolean Engine (CSG)

This directory contains the C++ boolean operation engine for Constructive Solid Geometry (CSG) in JotCAD, powered by CGAL (Computational Geometry Algorithms Library).

## Contents

* `engine.h` — Core engine interface. Implements Union, Difference, Intersection, Clip, and Corefinement algorithms using CGAL's Exact Kernel.
* [SPEC.md](file:///home/brian/github/jotcad_ez/geo/boolean/SPEC.md) — Technical specification detailing vertex matching tolerances, manifold recovery rules, and performance constraints.

## Technical Details

The boolean engine operates on 3D Surface Meshes:
1. **Deduplication & Exact Coordinates**: Maps vertex coordinates to exact representations (via `CGAL::Point_3<EK>`) before insertion to prevent floating-point drift.
2. **Watertight Checks**: Asserts topological closure (`CGAL::is_closed`) and resolves non-manifold configurations (bowtie singularities).
3. **Triangulation**: Automatically performs constrained Delaunay triangulation (`CDT`) and polygon triangulation (`CGAL::Polygon_mesh_processing::triangulate_faces`) on boolean outputs to guarantee compatibility with graphics pipeline decoders.
