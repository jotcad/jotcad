# Simulation (sim)

This directory is dedicated to the simulation environment and configurations for JotCAD. It coordinates virtual hardware testing, mesh network simulations, and coupled landscape evolution models.

## Responsibilities

- **Landscape Evolution Models**: Models geomorphology, hydrology, climatology, and forest ecology.
- **Platform Simulation**: Orchestrates virtual hardware configurations (e.g., ESP32 simulation via Wokwi).
- **Mesh Telemetry Testing**: Simulates node interactions, message passing, and subscription notifications within a virtualized local mesh network.
- **Mock Interfaces**: Houses mock configurations and test suites for simulating physical sensors and actuators.

## Structure

### Landscape Simulation Design
- [README.md](file:///home/brian/github/jotcad/sim/README.md): Index and directory overview.
- [HYDROLOGY.md](file:///home/brian/github/jotcad/sim/HYDROLOGY.md): Surface and subsurface water flow (Darcy's Law), spring exfiltration, and bank stability (pore pressure slumping).
- [ECOLOGY.md](file:///home/brian/github/jotcad/sim/ECOLOGY.md): Vegetation functional types (grass vs. trees), light competition, carbon sequestration ($CO_2$ capture), and soil organic humification.
- [GEOMORPHOLOGY.md](file:///home/brian/github/jotcad/sim/GEOMORPHOLOGY.md): Weather splintering, Aeolian wind transport, grain size sorting, lithification (rock formation), and glacial erosion.
- [CLIMATOLOGY.md](file:///home/brian/github/jotcad/sim/CLIMATOLOGY.md): Temperature lapse rates, aspect insolation indexes, and wildfire dynamics.

### Hardware Simulation
- `pio/`: Contains the firmware development workspace, which runs local hardware simulation utilizing `wokwi.toml` and Wokwi CLI.
