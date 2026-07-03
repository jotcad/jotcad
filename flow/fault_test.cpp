#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"

struct FaultState {
    int step;
    std::vector<std::vector<float>> H_soil;
};

void export_fault_data(const std::string& filename, const std::vector<FaultState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const faultData = {\n";
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

    // Initialize with a gentle Perlin noise baseline
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.05f, y * 0.05f);
            g.H_soil[y][x] = pn * 2.00f; // gentle topography
        }
    }

    const int total_steps = 100;
    float dt = 1000.0f; // 1,000 years per step

    Phase tectonic_phase("Tectonic Faulting", dt, total_steps);
    // Place a diagonal fault line with a total slip throw of 5.5 meters
    tectonic_phase.add<TectonicFault>(15.0f, 15.0f, 65.0f, 65.0f, 5.5f, 0.35f);
    tectonic_phase.add<Landslide>(1.00f); // landslide dry repose angle stability

    std::vector<FaultState> history;

    // Record initial step 0
    history.push_back({0, g.H_soil});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : tectonic_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil});
    }

    export_fault_data("test_fault.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Fault tectonic simulation exported to test_fault.js" << std::endl;
    return 0;
}
