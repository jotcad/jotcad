# TODO: Edge-Collapse Based Simplification

## Overview
Port the legacy edge-collapse simplification algorithm to the new `geo/` kernel. This is essential for reducing mesh density while maintaining topological integrity and sharp features.

## Technical Requirements
- **Algorithm**: `CGAL::Surface_mesh_simplification::edge_collapse`
- **Cost/Placement Policy**: `GarlandHeckbert_probabilistic_triangle_policies`
- **Sharp Edge Preservation**: Detect sharp edges via dihedral angles and apply `Constrained_placement`.
- **Safety**: Use `Bounded_normal_change_placement` to prevent face flipping.
- **Precision**: Perform simplification in `IK` (Inexact Kernel) for performance, then rehydrate/repair in `EK` (Exact Kernel).

## Source Material (Legacy)
- [Advanced Implementation (Sharp Edges, Bounded Normals)](../legacy/JSxCAD/algorithm/cgal/simplify_util.h)
- [Top-level Simplify Logic (Mesh vs. PWH)](../legacy/geometry/Simplify.h)
- [Basic Edge-Collapse Wrapper](../legacy/geometry/simplify_util.h)

## Implementation Plan
1. [ ] Create `geo/ops/simplify_op.h` following the `Processor` pattern.
2. [ ] Implement `simplify_util.h` in `geo/fix/` or `geo/algorithms/`.
3. [ ] Integrate sharp edge detection using `CGAL::approximate_dihedral_angle`.
4. [ ] Add `jot/Simplify` schema and register it in the UX catalog.
5. [ ] Verify with unit tests asserting topological closure and volume stability.
