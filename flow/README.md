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
This runs the high-speed **Hexagonal Grid Landscape Evolution Test** (in under 4 seconds), verifying:
1. **Soil & Hydrological Budgets**: Asserts that Net Mass Change, Average Vegetation, Max Discharge, and Active River Cells match the reference baseline.
2. **Visual Layout Verification**: Generates the 2D Top View in-memory and compares it pixel-by-pixel against the baseline reference.

### Baseline Generation
To record or update the visual and budget reference baseline, run the regression test binary with the `--generate` flag:
```bash
g++ -O3 -std=c++17 flow/hex_regression_test.cpp -o flow/hex_regression_test -lcrypto
./flow/hex_regression_test --generate
```

### Visual Regression Dashboard
A local HTTPS web server serves an interactive visual regression dashboard. 
1. Start the server (if not already running):
   ```bash
   node flow/server.js
   ```
2. Open the dashboard in your browser:
   [https://localhost:8085/regression_dashboard.html](https://localhost:8085/regression_dashboard.html)

The dashboard renders the final top-view simulation image on the left and a compact, 3-column table on the right displaying all parameters, verification status, and delta metrics.

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
*   [hex_climate.h](file:///home/brian/github/jotcad/flow/hex_climate.h): Registry holding climate profiles (Subtropical Highland, Arid Desert, Boreal Forest, Tropical Rainforest, Mediterranean Scrub, Tundra, and Temperate Deciduous Forest).
*   [hex_stream_test.cpp](file:///home/brian/github/jotcad/flow/hex_stream_test.cpp): Scenario runner executing 1,000 steps ($dt = 1,000\text{ years}$, $R_{\text{hex}} = 600\text{ m}$).
*   [hex_regression_test.cpp](file:///home/brian/github/jotcad/flow/hex_regression_test.cpp): Regression test suite validating budgets, vegetation coverage, and top-view visual pixels.

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
7. **Cartography, 3D Mating & Contour Rendering**:
   * Corner coordinates of the software rasterizer are evaluated by averaging adjacent cell centers ($vx = \sum cx_k / 3.0$), ensuring bit-identical screen coordinate mating with zero gaps or overlaps.
   * Vertical outer walls close the boundary down to $Z = -100\text{ m}$ on all visible edges (directions 0 to 3).
   * **Corrected Y-Depth Sorting**: Fixed the projection depth equation:
     $$\text{depth} = -(Y - y_{\text{mid}}) \cdot \text{scale\_xy} + Z \cdot \text{scale\_z}$$
     This assigns closer objects a larger depth value, resolving foreground/background occlusion inversion under $Z$-buffering.
   * **Thin 1px Vector Contour Lines**: Extracted contour lines at $100\text{ m}$ intervals using triangle-edge intersections (isoline marching), and rasterized them as thin, solid, exactly 1-pixel-wide lines using a $Z$-buffered Bresenham line algorithm with a $+0.5$ bias to prevent Z-fighting self-occlusion.
    * **Vibrant Forest Blending**: Reordered the rendering pipeline to apply the $35\%$ opaque arability tint directly to the base substrate (sand/rock) *first*, and overlay vegetation *second*. This keeps forests a pure, vibrant dark green and prevents muddy color mixing.

### Land Cover & Ecosystem Metrics
The regression suite and dashboard monitor the following percentage coverages:
1. **Open Water**: Standing surface water bodies (lakes and rivers) where depth exceeds `lake_threshold`.
2. **Wetland Coverage**: Saturated peat bogs, muskeg, or swamp undergrowth where groundwater table is within $10\text{ cm}$ of the surface (but not flooded as an open lake).
3. **Arable Coverage**: High-suitability agricultural land where the product of soil depth, flatness, and vegetation density exceeds a $10\%$ threshold.
4. **Forest Coverage**: Densely vegetated areas (vegetation $\ge 50\%$) with soil thickness $\ge 5\text{ cm}$.
5. **Grass Coverage**: Moderately vegetated areas (vegetation between $5\%$ and $50\%$) with soil thickness $\ge 5\text{ cm}$.
6. **Bare Soil Coverage**: Bare ground (vegetation $\le 5\%$) with soil thickness $\ge 5\text{ cm}$.
7. **Bedrock Coverage**: Exposed bedrock exposures where soil thickness is under $5\text{ cm}$.
