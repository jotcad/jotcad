# Mesh Integrity & Repair Module (`geo/fix`)

This module provides utilities to verify and enforce topological mesh integrity, manifold properties, and solid/watertight geometry inside JotCAD.

## Directory Index

* **[repair.h](file:///home/brian/github/jotcad_ez/geo/fix/repair.h)**: Implements Strategy I (Separation): checks for topological ambiguity (`is_geometry_unambiguous`), verifies solid properties (`is_geometry_solid`), and resolves singularities via Umbrella Splitting & Geometric Locking (`make_geometry_unambiguous`).
* **[bridge.h](file:///home/brian/github/jotcad_ez/geo/fix/bridge.h)**: Implements Strategy II (Bridging): resolves zero-volume contact singularities by fusing coincident touching apexes into a continuous positive-volume neck (`bridge_zero_volume_touches`).
* **[SPEC.md](file:///home/brian/github/jotcad_ez/geo/fix/SPEC.md)**: Details the formal specifications, design choices, 2D and 3D algorithms, and numerical constraints for manifold recovery.
* **[repair_test.cpp](file:///home/brian/github/jotcad_ez/geo/fix/repair_test.cpp)**: Contains unit tests validating collision-detection and ambiguity resolution on degenerate models (such as tetrahedrons touching cube facets at coincident coordinates).
* **[bridge_test.cpp](file:///home/brian/github/jotcad_ez/geo/fix/bridge_test.cpp)**: Contains unit tests validating bridge expansion across touching apexes.
