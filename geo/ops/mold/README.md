# Automated Multi-Piece Mold Decomposition Engine (`geo/ops/mold/`)

## Core Responsibility
This directory implements the researched, mathematically rigorous **Automated Multi-Piece Mold Decomposition Engine** for JotCAD. Given an arbitrary 3D watertight mesh and stock padding margins, it autonomously partitions the surrounding stock volume into $K$ interlocking, certified 2-manifold solid mold blocks $\{M_1, \dots, M_K\}$ using 3D Ruled Bisector Parting Sheets ($\Sigma$) and oriented slide prisms.

## Architectural Component Index

| File | Core Responsibility |
| :--- | :--- |
| **`types.h`** | Exact rational data structures (`UndercutCluster`, `MoldPiece`, `DSU`, `EdgeKey`, `ExactMesh`). |
| **`repair.h`** | Normalization, border welding, and Euler border cycle fan triangulation for watertight solids. |
| **`optimizer.h`** | Spherical search on $\mathbb{S}^2$ minimizing disjoint silhouette loops to find optimal draw vector $\vec{d}^*$. |
| **`visibility.h`** | Exact topological 1-ring primitive ID visibility raycasting and DSU undercut spatial clustering. |
| **`ribbon.h`** | Directed topological 1D loop chaining and 2-manifold quad parting sheet ($\Sigma$) extrusion in pure `FT`. |
| **`prism.h`** | Oriented prismatic slide column synthesis along cluster draw vectors $\vec{d}_{\text{insert}}$. |
| **`partition.h`** | Monolithic master stock box creation, single CSG cavity difference, and open surface volume slicing. |
| **`verify.h`** | Authoritative swept-volume demoldability assertion and back-draft hook verification. |
