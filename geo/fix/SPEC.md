# Specification: Universal Zero-Volume Contact & Kiss Resolution (`geo/fix/kiss.h`)

## 1. Objective
Provide a mathematically complete, robust, and unified module to detect and resolve **Zero-Volume Contacts (Kissing Singularities)** in 3D CAD meshes. The system supports dual engineering intents:
1. **Parting (`KissMode::PART`)**: Carves a deterministic physical clearance channel ($2\delta = 0.02\text{ mm}$) to ensure independent mold separation and eliminate non-manifold self-intersections.
2. **Joining (`KissMode::WELD`)**: Adds a solid structural bridge ($2\delta = 0.02\text{ mm}$) to fuse touching bodies into a single watertight 2-manifold solid.

---

## 2. Complete 3D Boundary Contact Taxonomy ($3 \times 3$ Matrix)

In 3D Euclidean geometry, any polyhedral boundary consists exclusively of **0-cells (Vertices)**, **1-cells (Edges)**, and **2-cells (Faces)**. The symmetric pair matrix defines exactly 6 contact configurations:

| Contact Type | Features Involved | Contact Dimension | Minkowski Correction Tool |
| :--- | :--- | :---: | :--- |
| **Point-to-Point** | Vertex $V_1$ touches Vertex $V_2$ | **0D Point** | **3D Cube** ($[x \pm \delta, y \pm \delta, z \pm \delta]$) |
| **Point-to-Edge** | Vertex $V$ touches interior of Edge $E$ | **0D Point** | **3D Cube** at vertex coordinate $P$ |
| **Point-to-Face** | Vertex $V$ touches interior of Face $F$ | **0D Point** | **3D Cube** at vertex coordinate $P$ |
| **Edge-to-Edge** | Edge $E_1$ touches Edge $E_2$ (collinear or crossing) | **1D Segment** or **0D Point** | **Swept Prism / Box** ($[P_{\text{start}}, P_{\text{end}}] \pm \delta$) |
| **Edge-to-Face** | Edge $E$ lies flush on flat Face $F$ | **1D Segment** | **Swept Prism / Box** along contact segment |
| **Face-to-Face** | Face $F_1$ touches Face $F_2$ coplanar ($A > 0$) | **2D Polygon Patch** | **Extruded Slab** (Thickness $2\delta$ along normal $\vec{N}$) |

---

## 3. Core Principles & Invariants

### A. Principle of Least Distortion (Lower-Dimension Preference)
In asymmetric contacts (e.g. Point-to-Face or Edge-to-Face), the correction is applied to the **lower-dimensional feature** ($0\text{D} \to 1\text{D} \to 2\text{D}$):
- **Point-to-Face**: Blunting the 0D apex tip alters an infinitesimal volume ($O(\delta^3) \approx 10^{-6}\text{ mm}^3$) while keeping the 2D functional reference plane **100% flat, continuous, and untouched**.
- **Edge-to-Face**: Relieving the 1D knife edge ($O(L \cdot \delta^2)$) preserves the 2D reference plane completely flat.

### B. Prismatic Hulls & Cubical Extrusions in Pure `EK::FT`
All correction tools are exact convex polyhedra (cubes, collinear swept boxes, and extruded slabs):
- **Kernel Purity**: All coordinates are computed purely in exact rational `EK::FT` with zero trigonometric or square-root rounding.
- **Convexity Guarantee**: Convex prismatic tools ensure clean, well-conditioned Corefinement.
- **Bounded Envelope**: The Chebyshev radius ($L_\infty \le \delta$) guarantees zero geometric modification beyond the $\delta = 0.01\text{ mm}$ boundary.

### C. Topological Pre-Conditioning
1. **Unpinning**: `CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices` ensures every edge in the topological graph has degree 2 (zero points moved).
2. **Collinear Merging**: Contiguous collinear kissing segments are chained into maximal straight lines prior to tool generation, minimizing Boolean operations.

---

## 4. API Design (`geo/fix/kiss.h`)

```cpp
namespace jotcad::geo::fix {

enum class KissMode {
    PART, // Minkowski Difference: carves clearance gap (2*delta)
    WELD  // Minkowski Union: adds solid structural bridge (2*delta)
};

template <typename K = EK>
bool resolve_kissing_seams(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, 
    KissMode mode = KissMode::PART,
    typename K::FT delta = 0.01
);

template <typename K = EK>
inline bool separate_kissing_columns(CGAL::Surface_mesh<typename K::Point_3>& mesh, typename K::FT delta = 0.01);

template <typename K = EK>
inline bool weld_kissing_columns(CGAL::Surface_mesh<typename K::Point_3>& mesh, typename K::FT delta = 0.01);

} // namespace jotcad::geo::fix
```

---

## 5. Verification & Status
- **Implementation**: Fully established in [`geo/fix/kiss.h`](file:///home/brian/github/jotcad_ez/geo/fix/kiss.h).
- **Regression Suite**: Validated in [`geo/test/test_repair_wedge.cpp`](file:///home/brian/github/jotcad_ez/geo/test/test_repair_wedge.cpp) across 6 test cases:
  - Free Kissing Edges (0 collisions).
  - Real-world 345-vertex Bear Fixture (0 collisions).
  - Bridged Seams (0 collisions).
  - Watertight Minkowski Welding (0 collisions, closed 2-manifold solid).
- **Status**: [VERIFIED & COMMITTED] (Commit `58c78eb`).
