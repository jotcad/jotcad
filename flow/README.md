# Flow Solver Module

This directory contains implementation code and specifications for the grid-based (Eulerian) fluid flow velocity field solver.

## C++ Modular Solver Architecture

The solver has been refactored into single-responsibility header components, keeping all C++ source files strictly under the 300-line threshold.

### Core Modules
*   [perlin_noise.h](file:///home/brian/github/jotcad/flow/perlin_noise.h): Procedural noise generator for soil/terrain initialization.
*   [grid.h](file:///home/brian/github/jotcad/flow/grid.h): MAC staggered grids container housing terrain heights, surface/subsurface water, and face velocities.
*   [element.h](file:///home/brian/github/jotcad/flow/element.h): Abstract interface for step-based physics processes.
*   [orchestrator.h](file:///home/brian/github/jotcad/flow/orchestrator.h): High-level simulation registry and execution controller.

### Physics Elements
*   [precipitation.h](file:///home/brian/github/jotcad/flow/precipitation.h): Handles uniform rainfall rate accumulation.
*   [hydrodynamics.h](file:///home/brian/github/jotcad/flow/hydrodynamics.h): Solves shallow water equations with depth-dependent drag, velocity smoothing, and flux-limited upwind advection.
*   [erosion.h](file:///home/brian/github/jotcad/flow/erosion.h): Coupled soil moisture infiltration, Darcy subsurface flow, exfiltration seepage, and shear-stress-based bed erosion/deposition.
*   [landslide.h](file:///home/brian/github/jotcad/flow/landslide.h): Iterative terrain slope stability and soil slip repose angle enforcement.
*   [sink.h](file:///home/brian/github/jotcad/flow/sink.h): Internal boundary drain handling water and sediment removal with mass balance telemetry logging.

### Exporters & Runners
*   [stream_exporter.h](file:///home/brian/github/jotcad/flow/stream_exporter.h): Consolidates visualizer JS dataset compilation and BMP snapshot exporting.
*   [flow_solver.h](file:///home/brian/github/jotcad/flow/flow_solver.h): Unified include file that exposes the modular core element classes.
*   [stream_test.cpp](file:///home/brian/github/jotcad/flow/stream_test.cpp): High-level stream formation simulation scenario runner and regression test suite.

## Verification & Compilation

Run the verification test suite directly using the runner script:
```bash
./run_tests.sh
```
All code compiles cleanly and passes all mass conservation and velocity stability checks.

---

## Hexagonal Landscape Evolution Solver (v4.6.0-arable)

We have implemented a high-performance **Hexagonal Landscape Evolution Simulator** that models long-term geological weathering, fluvial routing, sediment transport, mass wasting, vegetation growth, and organic soil production over a 1,000,000-year time scale.

### Hexagonal Core Modules
*   [hex_grid.h](file:///home/brian/github/jotcad/flow/hex_grid.h): 2D axial-coordinate hexagonal grid container.
*   [hex_routing.h](file:///home/brian/github/jotcad/flow/hex_routing.h): High-performance upwind multi-flow drainage router and lake depression filler.
*   [hex_erosion.h](file:///home/brian/github/jotcad/flow/hex_erosion.h): Fluvial shear-stress-driven bed erosion and slope-dependent diffuse rain wash.
*   [hex_landslide.h](file:///home/brian/github/jotcad/flow/hex_landslide.h): Soil slope stability wasting enforcing a dynamic repose angle.
*   [hex_vegetation.h](file:///home/brian/github/jotcad/flow/hex_vegetation.h): Dual-directional soil production (weathering + organic litter), and altitude-temperature treeline cooling.
*   [hex_exporter.h](file:///home/brian/github/jotcad/flow/hex_exporter.h): 3D isometric software rasterizer rendering seam-free, closed block terrain models with arability overlays.
*   [hex_visualizer.html](file:///home/brian/github/jotcad/flow/hex_visualizer.html): Interactive 2D/3D web simulation dashboard with calibrated river color hierarchies.
*   [hex_stream_test.cpp](file:///home/brian/github/jotcad/flow/hex_stream_test.cpp): Scenario runner executing 1,000 steps ($dt = 1,000\text{ years}$, $R_{\text{hex}} = 600\text{ m}$).

### Physical Formulation
1. **Fluvial Routing**: Discharges are routed along downslope neighbor gradients using multi-flow direction weights. Depressions and craters are filled dynamically using a priority-queue lake filler.
2. **Physical Scale Alignment**:
   * Set `HEX_RADIUS = 600.0f` meters. This matches the horizontal grid scale with the vertical projection scale ($15.0\text{ px} \times 0.41 / 600\text{ m} \approx 0.01 = heightScale$), aligning physical slope tangents ($10\% - 30\%$) with the visually perceived slopes.
   * Enables landslide relaxation (repose angle $>15\%$) and rain-wash to operate at realistic geomorphic strengths.
3. **Slope-Dependent Rain Wash**:
   * Added diffuse rain splash erosion scaling quadratically with slope:
     $$\text{rain\_wash} = 3.5 \cdot \left(\frac{\text{Slope}}{0.15}\right)^2$$
4. **Dual-Directional Soil Production**:
   * **Bottom-Up Bedrock Weathering**: Chemical/physical weathering of bedrock substrate into mineral soil, lowering $H_{\text{bedrock}}$:
     $$\Delta h_{\text{weathering}} = P_0 \cdot \left(0.1 + 0.9 \cdot V\right) \cdot e^{-k \cdot h_{\text{soil}}} \cdot dt$$
   * **Top-Down Organic Accumulation**: Accumulation of leaf litter and humus from carbon capture, raising the surface elevation $H_{\text{soil}}$:
     $$\Delta H_{\text{soil}} = R_{\text{org}} \cdot V \cdot dt$$
5. **Altitude-Temperature Cooling (Lapse Rate)**:
   * Restricts vegetation growth at high altitudes to simulate alpine timberline zones:
     $$\text{Temp} = 25.0^\circ\text{C} - \frac{H}{85.0\text{ m}}$$
     $$\text{temp\_factor} = \max\left(0.0\text{f}, \min\left(1.0\text{f}, \frac{\text{Temp} - 10.0\text{f}}{8.0\text{f}}\right)\right)$$
   * Restricts bare bedrock strictly to the highest summits ($>950\text{ m}$), keeping lower flanks forested.
6. **Discrete Biome Classification (Forest XOR Grass)**:
   * Replaced continuous biome gradients with a step threshold. Lowland cells are strictly classified as Forest (`RGB 20, 120, 45`) if $\text{moisture\_factor} \ge 0.35$, otherwise Grassland (`RGB 185, 230, 85`), ensuring sharp, legible vegetation boundaries.
7. **Cartography & 3D Mating**:
   * Corner coordinates of the software rasterizer are evaluated by averaging adjacent cell centers ($vx = \sum cx_k / 3.0$), ensuring bit-identical screen coordinate mating with zero gaps or overlaps.
   * Vertical outer walls close the boundary down to $Z = -100\text{ m}$ on all visible edges (directions 0 to 3).
   * **Vibrant Forest Blending**: Reordered the rendering pipeline to apply the $35\%$ opaque arability tint directly to the base substrate (sand/rock) *first*, and overlay vegetation *second*. This keeps forests a pure, vibrant dark green and prevents muddy color mixing.

