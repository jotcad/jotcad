# 2D Packing & Nesting

This directory provides tools for arranging 2D shapes (islands/patches) into manufacturing sheets (bins) to maximize material yield.

## Core Strategy
JotCAD uses a two-stage approach to flat-pattern generation:
1.  **Unfolding (`geo/unfold`)**: Decomposes 3D geometry into flat 2D clusters.
2.  **Packing (`geo/pack`)**: Arranges these clusters efficiently onto one or more 2D sheets.

## Architecture
- `engine.h`: High-level wrapper for the `libnest2d` nesting library.
- `sheet.h`: Defines sheet dimensions, margins, and placement constraints.

## Interleaving (Future)
The packing engine is designed to be "interleavable" with the unfolding process. Instead of a linear pipeline (Unfold -> Pack), the `unfold::Clusterer` can query the `pack::Engine` during its greedy growth phase to decide whether joining a face into a cluster will result in a shape that is too large or too inefficient to pack.

## Library: libnest2d
We utilize `libnest2d` (header-only) for its robust support for:
- **Irregular Nesting**: Uses No-Fit Polygons (NFP) for precise placement of non-convex shapes.
- **Multiple Bins**: Automatically overflows into additional sheets if the primary sheet is full.
- **Custom Backends**: Currently configured to use a compatible exact-kernel logic for stability.
