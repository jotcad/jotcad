# Simulation (sim)

This directory is dedicated to the simulation environment and configurations for JotCAD. It coordinates virtual hardware testing, mesh network simulations, and mock sensor data pipelines.

## Responsibilities

- **Platform Simulation**: Orchestrates virtual hardware configurations (e.g., ESP32 simulation via Wokwi).
- **Mesh Telemetry Testing**: Simulates node interactions, message passing, and subscription notifications within a virtualized local mesh network.
- **Mock Interfaces**: Houses mock configurations and test suites for simulating physical sensors and actuators.

## Structure

Currently, simulation targets are defined across:
- `pio/`: Contains the firmware development workspace, which runs local hardware simulation utilizing `wokwi.toml` and Wokwi CLI.
- `sim/`: Future home for consolidated, unified simulation harnesses, test benches, and virtual network topology configurations.
