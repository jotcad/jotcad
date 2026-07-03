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
