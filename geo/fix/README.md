# Mesh Integrity & Repair Module (`geo/fix`)

This module provides utilities to verify and enforce topological mesh integrity, manifold properties, and solid/watertight geometry inside JotCAD.

## Directory Index

* **[repair.h](file:///home/brian/github/jotcad_ez/geo/fix/repair.h)**: Implements Strategy I (Separation): checks for topological ambiguity (`is_geometry_unambiguous`), verifies solid properties (`is_geometry_solid`), and resolves singularities via Umbrella Splitting & Geometric Locking (`make_geometry_unambiguous`).
* **[predicates.h](file:///home/brian/github/jotcad_ez/geo/fix/predicates.h)**: Exact Rational Geometric Predicates (`is_point_on_segment`, `is_point_in_triangle`, `is_point_near_plane`, `do_triangles_form_convex_quad`): lightweight exact predicates built on native CGAL `do_intersect` in pure `EK::FT`.
* **[tool_builder.h](file:///home/brian/github/jotcad_ez/geo/fix/tool_builder.h)**: Minkowski Tool Generation (`make_box_mesh`, `merge_collinear_segments`): collinear segment chaining and exact rational bounding box / convex hull construction.
* **[assert_mesh.h](file:///home/brian/github/jotcad_ez/geo/fix/assert_mesh.h)**: Hard Topological & Geometric Integrity Assertions (`assert_well_formed_mesh`): strictly verifies that surface meshes have no uncollected garbage, are closed 2-manifolds, have zero self-intersections, and correctly bound a positive volume before corefinement.
* **[kiss.h](file:///home/brian/github/jotcad_ez/geo/fix/kiss.h)**: Universal Zero-Volume Kissing Seam Resolution (`resolve_kissing_seams`, `separate_kissing_columns`, `weld_kissing_columns`): unpins non-manifold 4-face edges into 2-manifold topological edges and resolves kissing seams via dual Minkowski Parting (Difference) and Joining (Union) Corefinements.
* **[SPEC.md](file:///home/brian/github/jotcad_ez/geo/fix/SPEC.md)**: Details the formal specifications, design choices, 2D and 3D algorithms, and numerical constraints for manifold recovery.
* **[repair_test.cpp](file:///home/brian/github/jotcad_ez/geo/fix/repair_test.cpp)**: Contains unit tests validating collision-detection and ambiguity resolution on degenerate models.
* **[bridge_test.cpp](file:///home/brian/github/jotcad_ez/geo/fix/bridge_test.cpp)**: Contains unit tests validating bridge expansion across touching apexes.
