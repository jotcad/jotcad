#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "tectonic_strikeslip.h"

struct StrikeSlipState {
    int step;
    std::vector<std::vector<float>> H_soil;
};

void export_strikeslip_data(const std::string& filename, const std::vector<StrikeSlipState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const strikeslipData = {\n";
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

    // Initialize with a prominent organic horizontal mountain ridge running west to east
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            // Wiggle the ridge center line using Perlin noise
            float ridge_center = 40.0f + 5.5f * perlin.noise(x * 0.08f, 0.0f);
            float dist_to_center = std::abs((float)y - ridge_center);
            float macro = std::max(0.0f, 4.0f - dist_to_center * 0.40f);
            float micro = 0.20f * perlin.noise(x * 0.15f, y * 0.15f);
            g.H_soil[y][x] = macro + micro;
        }
    }

    const int total_steps = 100;
    float dt = 1000.0f; // years per step

    Phase tectonic_phase("Strike-Slip Shearing", dt, total_steps);
    // Vertical fault line segment at x = 40 (from (40, 5) to (40, 75))
    // Total slip = 12.0 meters, sharpness k = 0.25 (less sharp, more realistic shear width)
    tectonic_phase.add<TectonicStrikeSlip>(40.0f, 5.0f, 40.0f, 75.0f, 12.0f, 0.25f, 0.0f, 0.0f);

    std::vector<StrikeSlipState> history;
    history.push_back({0, g.H_soil});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : tectonic_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil});
    }

    export_strikeslip_data("test_strikeslip.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Strike-Slip simulation exported to test_strikeslip.js" << std::endl;
    return 0;
}
