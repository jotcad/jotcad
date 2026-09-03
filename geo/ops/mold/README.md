# Automated Multi-Piece Mold Decomposition Engine (`geo/ops/mold/`)

## Core Responsibility
This directory implements the modular, mathematically rigorous **Automated Multi-Piece Mold Decomposition Engine** for JotCAD. Given an arbitrary 3D watertight mesh and stock padding margins, it autonomously partitions the surrounding stock volume into $K$ interlocking, certified 2-manifold solid mold blocks $\{M_1, \dots, M_K\}$ using 3D Upper Envelope height fields and minimal-volume OBB trimming.

## Architectural Component Index

| File | Lines | Core Responsibility |
| :--- | :--- | :--- |
| **`types.h`** | ~100 | Exact rational data structures (`DSU`, `EdgeKey`, `UndercutCluster`, `MoldParams`, `MoldPiece`, `build_box_geo`). |
| **`rotation.h`** | ~40 | Pure exact rational rotation calculations via `CGAL::rational_rotation_approximation`. |
| **`walls.h`** | ~80 | Monotonic zip vertical wall triangulation between height lists. |
| **`diagnostics.h`** | ~140 | Umbrella topology audits, polygon soup adjacency verification, and self-intersection reporting. |
| **`envelope.h`** | ~215 | Exact 3D Upper envelope calculation on visible component patches via `CGAL::upper_envelope_3`. |
| **`wedge.h`** | ~300 | Solid 3D wedge mesh synthesis via 2D CDT floor/ceiling, monotonic cliff walls, and `fix::repair_solid_soup`. |
| **`visibility.h`** | ~20 | Aggregating facade for mold visibility and envelope submodules. |
| **`obb.h`** | ~140 | Minimal-volume Oriented Bounding Box (`OrientedBox`, `compute_min_volume_obb`) aligned with piece draw vectors. |
| **`repair.h`** | ~45 | Exact coordinate normalization and watertight solid mesh repair (`normalize_and_repair_solid`). |
| **`optimizer.h`** | ~130 | Geometry-informed candidate scanning (corner normals, edge bisectors, face normals, Fibonacci lattice). |
| **`assembly.h`** | ~110 | Minimal-volume OBB trimming, stationary base block extraction (`pull_vector = "0 0 0"`), and scene graph assembly. |
| **`verify.h`** | ~40 | Authoritative swept-volume demoldability assertions (`verify_piece_demoldability`). |

## Operating Pipeline

1. **Geometry Aggregation & Repair** (`repair.h`): Aggregates world geometry recursively across the scene graph and heals boundary cracks into a watertight 2-manifold exact solid.
2. **Progressive Direction Optimization & Envelope Extraction** (`optimizer.h`, `envelope.h`): Evaluates candidate draw directions on $\mathbb{S}^2$, computing the 3D Upper Envelope solid wedge for the largest demoldable patch.
3. **Unconstrained Stock Carving**: Intersects each envelope wedge with the conservative stock envelope and carves the model cavity via CGAL exact boolean corefinement.
4. **OBB Trimming & Stationary Remainder Extraction** (`assembly.h`): Computes the minimal-volume OBB aligned with all extracted draw vectors, trims each piece, and extracts any uncarved stock remainder as a stationary base foundation block (`pull_vector = "0 0 0"`).
5. **Demoldability Assertion & Scene Graph Synthesis** (`assembly.h`, `verify.h`): Tags each piece with its withdrawal vector, assigns 50% opacity and 6-digit hex colors, applies explosion offsets along withdrawal vectors, and attaches the centered model cavity.
