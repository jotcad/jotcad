# TODO: Mold Decomposition Rational Coordinate Precision & OBB Optimization

## Overview
During automated multi-piece mold decomposition (`jot/mold`), intermediate and final mesh coordinates can experience **exponential rational bit-growth** (accumulating 500 to 2,500 decimal digits in exact rational fractions $\frac{N}{D}$). While topologically and geometrically certified within CGAL's exact rational kernel (`EK::FT`), this bit explosion inflates serialized `.jot` file payloads (~500 KB for simple shapes) and burdens downstream CSG operations.

## Root Cause Analysis
1. **Unconstrained OBB Rotation**:
   [`mold::compute_min_volume_obb`](../geo/ops/mold/obb.h) evaluates candidate bounding box orientations across 32 Fibonacci sphere angles and 12 in-plane rotational samples. For symmetric CAD objects (such as the 2-Way Planar Cross), a $45^\circ$ diamond-oriented box encloses less volume ($(20\sqrt{2})^2 \times 10 = 8,000\,\text{mm}^3$) than an axis-aligned bounding box ($30^2 \times 10 = 9,000\,\text{mm}^3$, an 11.1% volume reduction).
2. **Cascaded Exact 3D Corefinements**:
   Rotating stock planes introduces irrational trigonometric coefficients (e.g. $\frac{\sqrt{2}}{2}$ approximations with $2^{48}$ or $10^6$ denominators). When CGAL exact corefinement computes intersections (`corefine_intersection`) and differences (`corefine_difference`) against the model cavity and harmonic wedges, each intersection vertex is computed via $3 \times 3$ plane determinants, multiplying the bit-length of coordinates at every boolean stage ($2^{48} \to 2^{144} \to 2^{500} \to 2^{2500}$).
3. **Frontend Precision Overflow**:
   When serialized into `.jot` text assets, coordinates with $> 308$ digits overflow standard IEEE-754 `parseFloat(n) / parseFloat(d)` into `Infinity / Infinity = NaN`, causing Three.js to drop facets in the viewer unless decoded with `BigInt` scaling.

## Current Mitigations
- **Frontend**: [`web/src/render/GeometryDecoder.js`](../web/src/render/GeometryDecoder.js) implements the `BigInt` shift algorithm from [`ux/src/lib/ft.js`](../ux/src/lib/ft.js), scaling numbers exceeding 250 digits down to 15 significant digits before floating-point conversion, preventing `NaN` and restoring missing facets.
- **Harmonic Parting Surface**: [`geo/ops/mold/harmonic.h`](../geo/ops/mold/harmonic.h) rounds non-fixed solved heights to the nearest $0.01\,\text{mm}$ (`EK::FT(centi) / EK::FT(100)`), keeping parting surface $Z$-heights bounded.

## Planned Improvements
1. [ ] **Cardinal / Draw-Direction Bias in OBB**:
   - Bias [`compute_min_volume_obb`](../geo/ops/mold/obb.h) toward axis-aligned and draw-vector-aligned stock boxes unless an oblique OBB provides a substantial volume reduction (e.g. $> 25\%$).
   - For standard orthogonal features, prefer clean rational bounding planes ($u = \hat{X}, v = \hat{Y}, w = \hat{Z}$) with zero trigonometric rotation.
2. [ ] **Stock Box Coordinate Grid Snapping**:
   - Round OBB vertex coordinates and plane offsets to a clean physical grid (e.g. $0.01\,\text{mm}$ or $0.001\,\text{mm}$) before generating stock box meshes.
3. [ ] **Wedge Boundary Simplification**:
   - Ensure the outer rectangular extrusion boundary of `construct_harmonic_wedge` conforms to exact axis-aligned or rational coordinates without unnecessary oblique cuts.
4. [ ] **Review Reflection vs Rotation in `rotation.h` (Handedness & Bit-Growth)**:
   - [`mold::compute_exact_z_rotation`](../geo/ops/mold/rotation.h) currently cascades two Euler rotations ($\phi$ and $\theta$) using `CGAL::rational_rotation_approximation` with denominators up to $10^6$. Their product yields homogeneous denominators up to $10^{12}$, inflating coordinate bit lengths during envelope intersections.
   - **Review Reflection**: A single Householder reflection $H = I - 2 \frac{\mathbf{u}\mathbf{u}^T}{\mathbf{u}^T\mathbf{u}}$ across the bisector $\mathbf{u} = \mathbf{d} + \hat{Z}$ is an isometry that maps $\mathbf{d} \to \hat{Z}$ with small denominators. However, because $\det(H) = -1$, a single reflection is an *improper* isometry that reverses spatial handedness / orientation (flipping face normals and polygon vertex winding order).
   - **Evaluate Proper $SO(3)$ Formulations**: Investigate whether (a) a composition of two Householder reflections (via the Cartan-Dieudonné theorem) or (b) an exact Cayley transform / Rodrigues rotation parameterization can provide a proper rotation ($\det = +1$) with small rational denominators, avoiding the $10^{12}$ bit explosion of cascaded Euler approximations while maintaining orientation invariants.

