# Problem Specification: Zero-Volume Contact & Manifold Recovery

## 1. The Challenge: Topological Singularities
In CAD operations (specifically `cut`, `join`, and `rule`), geometric results often produce "zero-volume contacts" where distinct manifold components share a singular vertex or edge. 

While mathematically valid, these configurations are "unstable" because:
- **Spatial Deduplication**: Standard mesh kernels and "repair" algorithms merge coincident vertices, turning two independent manifold shells into a single **non-manifold** entity.
- **Boolean Failure**: Subsequent operations on non-manifold geometry often fail or produce unpredictable results.
- **Numerical Noise**: Near-zero distances at these contact points lead to precision errors in exact predicates.

## 2. Classes of Zero-Volume Contact

### A. Point-to-Point (The Bowtie)
- **2D**: Two polygons sharing a single vertex.
- **3D**: Two polyhedra (e.g., cones) sharing an apex.
- **Topological State**: Non-manifold vertex (the Link is not a single disk/cycle).

### B. Edge-to-Edge (The Hinge)
- **3D**: Two polyhedra sharing an edge but no adjacent faces.
- **Topological State**: Non-manifold edge (3+ faces sharing an edge).

### C. Surface-to-Surface (The Membrane)
- **3D**: Two polyhedra sharing a face with opposite normals (coincident faces).

### D. Point-to-Surface (The Apex/Pierce)
- **3D**: A vertex of one body touching the interior or boundary of a face of another body.
- **Degenerate Rule**: A ruled surface connecting a loop to a single point (creating a cone/pyramid).
- **Topological State**: Non-manifold vertex where the surface "pinches" to a zero-area contact.

## 3. Resolution Strategies

### I. Infinitesimal Separation (The "Split")
To prevent spatial merging, topologically distinct vertices must be moved apart.
- **Local Subdivision**: Before moving the vertex, subdivide the incident edges/faces within a small radius $\delta$ of the singularity.
- **Perturbation**: Displace the original vertex along its local manifold normal or "center of mass" to break the coordinate identity.
- **Benefit**: Preserves $99.9\%$ of the original face planarity.

### II. Non-Zero Volume Expansion (The "Bridge")
Convert the zero-volume contact into a physical manifold connection.
- **Capping**: "Clip" the touching tips and insert a small polygonal bridge (a neck) between them.
- **Benefit**: Extremely robust for physical manufacturing simulations; avoids "magic" zero-width connections.

### III. Recovery & Geometric Locking
Since we assume that **any vertices sharing a coordinate will eventually be merged** (by the kernel, a renderer, or an exporter), the geometry must "defend" its own manifoldness.

1.  **Topological Recovery**: Analyze the input "soup" or operation trace to determine which faces belong to the same manifold shell.
2.  **Geometric Locking**: 
    - For vertices belonging to different shells that share a coordinate, **separate them geometrically**.
    - Use the **Subdivision + Perturbation** strategy to ensure they have distinct points.
    - This "locks" the manifold state into the geometric data, making it immune to coordinate-based merging.

## 4. Scope of Implementation
- **2D (GPS)**: Handling the results of `jot/cut` and `jot/join`.
- **3D (Rule/Boolean)**: Handling apex-to-apex and edge-to-edge contacts in the native kernel.
