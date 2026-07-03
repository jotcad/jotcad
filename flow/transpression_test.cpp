#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "tectonic_strikeslip.h"

struct TranspressionState {
    int step;
    std::vector<std::vector<float>> H_soil;
};

void export_transpression_data(const std::string& filename, const std::vector<TranspressionState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const transpressionData = {\n";
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

    // Initialize with a gentle flat baseline terrain
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.05f, y * 0.05f);
            g.H_soil[y][x] = pn * 0.80f; // flat baseline plain
        }
    }

    const int total_steps = 100;
    float dt = 1000.0f; // years per step

    Phase tectonic_phase("Transpression & Transtension", dt, total_steps);
    // Vertical fault at x = 40 (from (40, 5) to (40, 75))
    // Total slip = 18.0 meters
    // Sharpness k = 0.35 (wider, more realistic shear zone)
    // Wobble amplitude = 5.0f (organic curves creating bends)
    // Uplift coefficient = 35.0f (generates transpressional hills and transtensional sag basins along the bends)
    tectonic_phase.add<TectonicStrikeSlip>(40.0f, 5.0f, 40.0f, 75.0f, 18.0f, 0.35f, 5.0f, 35.0f);
    tectonic_phase.add<Landslide>(0.65f); // landslide slope relaxation (33 degree angle)

    std::vector<TranspressionState> history;
    history.push_back({0, g.H_soil});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : tectonic_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil});
    }

    export_transpression_data("test_transpression.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Transpression simulation exported to test_transpression.js" << std::endl;
    return 0;
}
