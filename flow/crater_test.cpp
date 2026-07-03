#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "impact_crater.h"

struct CraterState {
    int step;
    std::vector<std::vector<float>> H_soil;
};

void export_crater_data(const std::string& filename, const std::vector<CraterState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const craterData = {\n";
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

    // Initial flat terrain with minor Perlin texture
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.05f, y * 0.05f);
            g.H_soil[y][x] = pn * 0.40f + 2.5f; // flat plain base
        }
    }

    const int total_steps = 60;
    float dt = 1.0f; // time step scale

    Phase impact_phase("Bolide Impact & Slumping", dt, total_steps);
    // Center at (40, 40), radius = 18.0, depth = 4.5, central rebound peak height = 2.0
    impact_phase.add<ImpactCrater>(40.0f, 40.0f, 18.0f, 4.5f, 2.0f);
    // Landslide repose slope = 0.60 (31 degrees) to simulate fractured rock slumping
    impact_phase.add<Landslide>(0.60f);

    std::vector<CraterState> history;
    history.push_back({0, g.H_soil});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : impact_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil});
    }

    export_crater_data("test_crater.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Impact crater simulation exported to test_crater.js" << std::endl;
    return 0;
}
