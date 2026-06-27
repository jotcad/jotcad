# Simulation (sim)

This directory is dedicated to the simulation environment and configurations for JotCAD. It coordinates virtual hardware testing, mesh network simulations, and physical landscape/hydrological simulations.

## Responsibilities

- **Platform Simulation**: Orchestrates virtual hardware configurations (e.g., ESP32 simulation via Wokwi).
- **Hydrological & Erosion Models**: Focuses on landscape evolution models, including surface rivers, subsurface aquifer flows, and bank collapse mechanics.
- **Mesh Telemetry Testing**: Simulates node interactions, message passing, and subscription notifications within a virtualized local mesh network.
- **Mock Interfaces**: Houses mock configurations and test suites for simulating physical sensors and actuators.

## Structure

- [README.md](file:///home/brian/github/jotcad/sim/README.md): Index and overview of the simulation space.
- [SIMULATION.md](file:///home/brian/github/jotcad/sim/SIMULATION.md): Comprehensive equations and algorithms for surface water, groundwater (Darcy's Law), and bank-slumping mechanics.
- `pio/`: Contains the firmware development workspace, which runs local hardware simulation utilizing `wokwi.toml` and Wokwi CLI.
