#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "tectonic_overthrust.h"

struct OverthrustState {
    int step;
    std::vector<std::vector<float>> H_soil;
};

void export_overthrust_data(const std::string& filename, const std::vector<OverthrustState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const overthrustData = {\n";
    out << "  grid_size: " << grid_size << ",\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        out << "      grid_H_soil: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].H_soil[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (s < history.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "};\n";
    out.close();
}

int main() {
    const int GRID_SIZE = 80;
    Grid g(GRID_SIZE);

    // Initial flat terrain with Perlin micro-undulations
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.05f, y * 0.05f);
            g.H_soil[y][x] = pn * 0.50f + 1.0f;
        }
    }

    const int total_steps = 120;
    float dt = 1500.0f; // years per step

    Phase collision_phase("Plate Collision Overthrust", dt, total_steps);
    // Diagonal plate boundary from (10, 15) to (70, 65)
    // Overriding plate climbs over with total slip = 15.0m
    // Thrust ramp angle = 22.0 degrees (leads to ~5.6m of vertical uplift)
    // Sharpness k = 0.40
    collision_phase.add<TectonicOverthrust>(10.0f, 15.0f, 70.0f, 65.0f, 15.0f, 22.0f, 0.40f);
    collision_phase.add<Landslide>(0.70f); // collapse of steep overriding thrust front

    std::vector<OverthrustState> history;
    history.push_back({0, g.H_soil});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : collision_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil});
    }

    export_overthrust_data("test_overthrust.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Overthrust simulation exported to test_overthrust.js" << std::endl;
    return 0;
}
