# Specification: Universal Zero-Volume Contact & Kiss Resolution (`geo/fix/kiss.h`)

## 1. Objective
Provide a mathematically complete, robust, and unified module to detect and resolve **Zero-Volume Contacts (Kissing Singularities)** in 3D CAD meshes. The system supports dual engineering intents:
1. **Parting (`KissMode::PART`)**: Carves a deterministic physical clearance channel ($2\delta = 0.02\text{ mm}$) to ensure independent mold separation and eliminate non-manifold self-intersections.
2. **Joining (`KissMode::WELD`)**: Adds a solid structural bridge ($2\delta = 0.02\text{ mm}$) to fuse touching bodies into a single watertight 2-manifold solid.

---

## 2. Complete 3D Boundary Contact Taxonomy ($3 \times 3$ Matrix)

In 3D Euclidean geometry, any polyhedral boundary consists exclusively of **0-cells (Vertices)**, **1-cells (Edges)**, and **2-cells (Faces)**. The symmetric pair matrix defines exactly 6 contact configurations:

| Contact Type | Features Involved | Contact Dimension | Unified CGAL Extraction | Consolidated Minkowski Tool |
| :--- | :--- | :---: | :--- | :--- |
| **Point-to-Point** | Vertex $V_1$ touches Vertex $V_2$ | **0D Point** | `Point_3` variant | **Convex 3D Cube** ($[x \pm \delta, y \pm \delta, z \pm \delta]$) |
| **Point-to-Edge** | Vertex $V$ touches interior of Edge $E$ | **0D Point** | `Point_3` variant | **Convex 3D Cube** at coordinate $P$ |
| **Point-to-Face** | Vertex $V$ touches interior of Face $F$ | **0D Point** | `Point_3` variant | **Convex 3D Cube** at coordinate $P$ |
| **Edge-to-Edge** | Edge $E_1$ touches Edge $E_2$ (collinear or crossing) | **1D Segment** or **0D Point** | `Segment_3` or `Point_3` | **Convex 1D Straight Prism** ($[P_1, P_2] \pm \delta$) |
| **Edge-to-Face** | Edge $E$ lies flush on flat Face $F$ | **1D Segment** | `Segment_3` variant | **Convex 1D Straight Prism** along contact segment |
| **Face-to-Face** | Face $F_1$ touches Face $F_2$ coplanar ($A > 0$) | **2D Polygon Patch** | `Triangle_3` / `std::vector<Point_3>` | **Convex 3D Triangular Prisms & Slabs** (Thickness $2\delta$) |

---

## 3. Core Principles & Invariants

### A. Unified Universal Intersection Extraction
Instead of fragmented ad-hoc loops over vertices and halfedges:
- **`CGAL::Polygon_mesh_processing::self_intersections`** extracts all intersecting non-adjacent face pairs $(F_1, F_2)$ via an exact AABB spatial search ($O(N \log N)$), eliminating false positives on connected mesh geometry.
- **`CGAL::intersection(Triangle_3, Triangle_3)`** in pure `EK` returns the exact variant type (`Point_3`, `Segment_3`, or `Triangle_3` / `std::vector<Point_3>`), unifying 0D, 1D, and 2D detection in a single mathematical predicate.

### B. The 3-Tier Convex Consolidation Hierarchy
To prevent Boolean fragmentation and duplicate intersection nodes in Corefinement:
1. **0D Point Consolidation**: Coincident and nearby collision coordinates are deduplicated via `std::set<Point_3>` into isolated **Convex 3D Cubes**.
2. **1D Polyline Consolidation**: Piecewise tessellated segments along straight contact boundaries are merged via `merge_collinear_segments` into maximal **Convex 1D Straight Prisms**.
3. **2D Convex Patch Consolidation**: Coplanar contact patches are partitioned into **triangles** (which are 100% strictly convex by definition) or merged into convex planar polygons, and extruded along normal $\vec{N}$ by $\pm\delta$ into **Convex 3D Triangular Prisms (Wedges)**. This guarantees zero over-cutting into non-contact cavities or holes.

### C. Principle of Least Distortion (Lower-Dimension Preference)
In asymmetric contacts (e.g. Point-to-Face or Edge-to-Face), the lower-dimensional feature is prioritized ($0\text{D} \to 1\text{D} \to 2\text{D}$):
- **Point-to-Face**: Relieving the 0D apex tip alters an infinitesimal volume ($O(\delta^3) \approx 10^{-6}\text{ mm}^3$) while keeping the 2D functional reference plane **100% flat, continuous, and untouched**.
- **Edge-to-Face**: Relieving the 1D knife edge ($O(L \cdot \delta^2)$) preserves the 2D reference plane completely flat.

### D. Exact Rational Kernel Purity (`EK::FT`)
- All tool vertices, plane equations, and Minkowski envelopes are computed in pure rational `EK::FT`.
- Zero float-to-double conversions in inner loops.
- Bounded Chebyshev radius ($L_\infty \le \delta = 0.01\text{ mm}$).

---

## 4. Architecture & Pipeline

```
  Step 1: Universal Feature Extraction
  ------------------------------------
  PMP::self_intersections(mesh) 
      └── CGAL::intersection(tri1, tri2)
            ├── Point_3                ──> contact_points (std::set)
            ├── Segment_3              ──> raw_segments
            └── Polygon / Triangle_3   ──> convex_triangles / patches

                                  │
                                  ▼
  Step 2: Intelligent Convex Consolidation
  ----------------------------------------
  - merge_collinear_segments(raw_segments) ──> Maximal 1D Prisms
  - Triangulation of Coplanar Patches      ──> Convex 2D Triangular Prisms

                                  │
                                  ▼
  Step 3: Minkowski Tool Building & Boolean Corefinement
  -------------------------------------------------------
  build_minkowski_tools(points, maximal_segments, convex_patches, delta)
      └── Applies Difference (PART) or Union (WELD) via CGAL Corefinement.
```

---

## 5. Verification Suite (`geo/test/test_repair_wedge.cpp`)

| Test Fixture | Combination Type | Expected Result | Verified Status |
| :--- | :--- | :---: | :---: |
| **TEST 1** | Free Kissing Edge | 0 Collisions | ✅ **PASSED** |
| **TEST 4** | Bear CAD Fixture (345 Vertices) | 0 Collisions | ✅ **PASSED** |
| **TEST 6** | Dual Minkowski Welding (WELD) | 0 Collisions, Watertight | ✅ **PASSED** |
| **TEST 7** | Point-to-Point (Pyramids Apex-to-Apex) | 0 Collisions | ✅ **PASSED** |
| **TEST 8** | Point-to-Edge (Apex on Cube Edge) | 0 Collisions | ✅ **PASSED** |
| **TEST 9** | Point-to-Face (Apex on Cube Face) | 0 Collisions | ✅ **PASSED** |
| **TEST 10** | Edge-to-Face (Prism Knife on Cube Face) | 0 Collisions | 🟡 Red-to-Green |
| **TEST 11** | Face-to-Face (Coplanar Cubes) | 0 Collisions | ✅ **PASSED** |
