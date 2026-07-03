#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "volcano.h"

struct VolcanoState {
    int step;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface; // holds lava depth
};

void export_volcano_data(const std::string& filename, const std::vector<VolcanoState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const volcanoData = {\n";
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
        out << "      ],\n";
        out << "      grid_h_surface: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].h_surface[y][x];
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

    // Initial mountain slope running from west to east with micro-ridges
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float macro = perlin.noise(x * 0.04f, y * 0.04f);
            float micro = 0.22f * perlin.noise(x * 0.22f, y * 0.22f); // micro-channel steering
            g.H_soil[y][x] = (macro + micro) * 1.50f + 2.0f - (float)x * 0.05f;
        }
    }

    const int total_steps = 150;
    float dt = 0.5f; // time step for viscous flow

    Phase volcanic_phase("Volcanic Eruption", dt, total_steps);
    // Vent at (25, 40), eruption_rate = 8.0, viscosity = 0.05, cooling = 0.01, solidification = 0.25
    volcanic_phase.add<Volcano>(25, 40, 8.0f, 0.05f, 0.01f, 0.25f);
    volcanic_phase.add<Landslide>(0.70f); // collapse of steep volcanic dome to 35 degree repose angle

    std::vector<VolcanoState> history;
    history.push_back({0, g.H_soil, g.h_surface});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : volcanic_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil, g.h_surface});
    }

    export_volcano_data("test_volcano.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Volcanic simulation exported to test_volcano.js" << std::endl;
    return 0;
}
