# Research: Polyhedral Unfolding and 2D Packing

## Overview
Unfolding a 3D polyhedron into a 2D layout (a "net") for manufacturing (paper-craft, CNC, sheet metal) is a dual-stage problem:
1. **Unfolding**: Determining which edges to cut/keep to flatten the surface.
2. **Packing (Nesting)**: Arranging the resulting 2D patches to maximize material yield.

## 1. Unfolding Approaches

### Legacy: `mu3d` (Simulated Annealing)
The legacy `mu3d` library uses a randomized search:
- **Algorithm**: Mutates weights on the dual graph and computes a Minimum Spanning Tree (MST).
- **Metric**: Minimizes self-overlap area and maximizes Bounding Box compactness of a *single* net.
- **Weakness**: Fails on shapes that cannot be unfolded into a single non-overlapping piece. Non-deterministic and uses floating-point math.

### Proposed: Greedy Topological Clustering
Instead of forcing a single net, we treat unfolding as a clustering problem:
- **Algorithm**: Start with $N$ independent faces. Greedily join faces across shared edges.
- **Join Criteria**:
  - No self-overlap in 2D.
  - High "Solidity" (Area / Bounding Box Area).
  - Fits within target sheet dimensions.
- **Benefit**: Handles non-convex shapes by automatically splitting them into multiple efficient "islands." Deterministic and compatible with Exact Rational Kernels.

## 2. Packing Libraries

### `libnest2d`
A modern C++11 header-only library (used in PrusaSlicer).
- **Core**: Uses **No-Fit Polygon (NFP)** for irregular placement.
- **Backends**: Typically uses **Clipper** and **Boost.Geometry**.
- **Performance**: Robust and fast; handles convex and some non-convex polygons.

### `libnesting` (CGAL-Based)
*Note: Further investigation needed to locate this specific header library, as it appears to be a specialized CGAL-leveraging tool for Minkowski-sum based nesting.*

## 3. Algorithmic Recommendations for JotCAD

1. **Topological Integration**: Unfolding should be a "Promoter" that preserves the link between the 2D face and the 3D source edge (for fold-line marking).
2. **Minkowski-Based NFP**: Leverage CGAL's `Minkowski_sum_2` for precise NFP calculation if exactness is required over raw speed.
3. **Hybrid Model**: Use Greedy Clustering to generate patches, then pass those patches to a Nesting engine (`libnest2d`) for final sheet layout.
4. **Deterministic Seed**: Use a stable topological sort (e.g., face index or CID) to ensure that the same geometry always produces the same unfolding.
