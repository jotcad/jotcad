# Mesh Integrity & Repair Module (`geo/fix`)

This module provides utilities to verify and enforce topological mesh integrity, manifold properties, and solid/watertight geometry inside JotCAD.

## Directory Index

* **[repair.h](file:///home/brian/github/jotcad_ez/geo/fix/repair.h)**: Implements functions to check for topological ambiguity (`is_geometry_unambiguous`), check for closed solid properties (`is_geometry_solid`), and automatically resolve topological singularities (`make_geometry_unambiguous`) using Umbrella Splitting & Geometric Locking.
* **[SPEC.md](file:///home/brian/github/jotcad_ez/geo/fix/SPEC.md)**: Details the formal specifications, design choices, 2D and 3D algorithms, and numerical constraints for manifold recovery.
* **[repair_test.cpp](file:///home/brian/github/jotcad_ez/geo/fix/repair_test.cpp)**: Contains unit tests validating collision-detection and ambiguity resolution on degenerate models (such as tetrahedrons touching cube facets at coincident coordinates).
