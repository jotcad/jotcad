# 2D Packing & Nesting

This directory provides tools for arranging 2D shapes (islands/patches) into manufacturing sheets (bins) to maximize material yield.

## Core Strategy
JotCAD uses a two-stage approach to flat-pattern generation:
1.  **Unfolding (`geo/unfold`)**: Decomposes 3D geometry into flat 2D clusters.
2.  **Packing (`geo/pack`)**: Arranges these clusters efficiently onto one or more 2D sheets.

## Architecture
- `packaide_engine.h`: The primary nesting engine based on the Packaide algorithm.
- `packaide_core.h`: Low-level geometric primitives for No-Fit Polygons (NFP) and Inner-Fit Polygons (IFP).
- `packaide_types.h`: CGAL-to-JotCAD bridge types using the exact construction kernel (`FT`).

## Precision & Stability
The engine utilizes **CGAL's Exact Construction Kernel (`FT`)** for all internal calculations. This ensures that exact-fit scenarios (e.g., a 10.0 unit part in a 10.0 unit sheet) are handled with mathematical certainty, preventing the numerical instability that commonly plagues nesting algorithms.

### Protocol Invariants
- **Strict Integrity**: Input geometry is never "fixed" or "sanitized" silently. The engine asserts that all input polygons are simple and free of duplicate sequential vertices.
- **2D Primacy**: All packing operations are performed in the XY plane. 3D transforms are projected before processing.
- **Top-Down Results**: Results are returned as an assembly where the sheet geometry itself is the first component (`components[0]`), providing a clear spatial context for the placed parts.

## Algorithms
- **No-Fit Polygons (NFP)**: Computed via Minkowski Sums of the stationary and orbiting parts.
- **Inner-Fit Polygons (IFP)**: Defines the boundary of the feasible placement region within the sheet.
- **Greedy Placement**: Parts are ordered by area and placed sequentially in the location that minimizes the current bounding box of the assembly (bottom-left bias).
