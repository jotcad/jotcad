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

## Hexagonal Landscape Evolution Solver (v4.5.0)

We have implemented a high-performance **Hexagonal Landscape Evolution Simulator** that models long-term geological weathering, fluvial routing, sediment transport, mass wasting, vegetation growth, and organic soil production over a 1,000,000-year time scale.

### Hexagonal Core Modules
*   [hex_grid.h](file:///home/brian/github/jotcad/flow/hex_grid.h): 2D axial-coordinate hexagonal grid container ($180 \times 150$ grid representing a $900 \times 750\text{ km}$ island territory).
*   [hex_routing.h](file:///home/brian/github/jotcad/flow/hex_routing.h): High-performance upwind multi-flow drainage router and lake depression filler.
*   [hex_erosion.h](file:///home/brian/github/jotcad/flow/hex_erosion.h): Fluvial shear-stress-driven bed erosion and sediment deposition solver.
*   [hex_landslide.h](file:///home/brian/github/jotcad/flow/hex_landslide.h): Soil slope stability wasting enforcing a dynamic repose angle.
*   [hex_vegetation.h](file:///home/brian/github/jotcad/flow/hex_vegetation.h): Dual-directional soil production (weathering + organic litter) and vegetation dynamics.
*   [hex_exporter.h](file:///home/brian/github/jotcad/flow/hex_exporter.h): 3D isometric software rasterizer rendering seam-free, closed block terrain models.
*   [hex_visualizer.html](file:///home/brian/github/jotcad/flow/hex_visualizer.html): Interactive 2D/3D web simulation dashboard.
*   [hex_stream_test.cpp](file:///home/brian/github/jotcad/flow/hex_stream_test.cpp): Scenario runner executing 1,000 steps ($dt = 1,000\text{ years}$).

### Physical Formulation
1. **Fluvial Routing**: Discharges are routed along downslope neighbor gradients using multi-flow direction weights. Depressions and craters are filled dynamically using a priority-queue lake filler.
2. **Dual-Directional Soil Production**:
   * **Bottom-Up Bedrock Weathering**: Chemical/physical weathering of bedrock substrate into mineral soil, lowering $H_{\text{bedrock}}$:
     $$\Delta h_{\text{weathering}} = P_0 \cdot \left(0.1 + 0.9 \cdot V\right) \cdot e^{-k \cdot h_{\text{soil}}} \cdot dt$$
   * **Top-Down Organic Accumulation**: Accumulation of leaf litter and humus from carbon capture, raising the surface elevation $H_{\text{soil}}$:
     $$\Delta H_{\text{soil}} = R_{\text{org}} \cdot V \cdot dt$$
3. **Cartography & 3D Mating**:
   * Corner coordinates of the software rasterizer are evaluated by averaging adjacent cell centers ($vx = \sum cx_k / 3.0$), ensuring bit-identical screen coordinate mating with zero gaps or overlaps.
   * Vertical outer walls close the boundary down to $Z = -100\text{ m}$ on all visible edges (directions 0 to 3).

