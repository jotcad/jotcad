#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "aeolian_dunes.h"

struct DuneState {
    int step;
    std::vector<std::vector<float>> H_soil;    // Bedrock
    std::vector<std::vector<float>> h_surface; // Sand depth
};

void export_dune_data(const std::string& filename, const std::vector<DuneState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const duneData = {\n";
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

    // Initial plain with two bedrock obstacles (mounds) to seed dune nucleation
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            g.H_soil[y][x] = 0.5f; // flat baseline elevation

            // Obstacle 1 (center-left)
            float dx1 = (float)x - 30.0f;
            float dy1 = (float)y - 30.0f;
            float r1 = std::sqrt(dx1*dx1 + dy1*dy1);
            if (r1 < 8.0f) {
                g.H_soil[y][x] += 2.0f * (1.0f - r1 / 8.0f);
            }

            // Obstacle 2 (center-right)
            float dx2 = (float)x - 50.0f;
            float dy2 = (float)y - 50.0f;
            float r2 = std::sqrt(dx2*dx2 + dy2*dy2);
            if (r2 < 8.0f) {
                g.H_soil[y][x] += 2.2f * (1.0f - r2 / 8.0f);
            }
        }
    }

    const int total_steps = 200;
    float dt = 0.4f; // step scale for wind transport speed

    Phase aeolian_phase("Aeolian Dune Transport", dt, total_steps);
    // input = 0.25, carry = 0.55, alpha = 12.0, repose = 0.62, relax = 1.50
    aeolian_phase.add<AeolianDunes>(0.25f, 0.55f, 12.0f, 0.62f, 1.50f);

    std::vector<DuneState> history;
    history.push_back({0, g.H_soil, g.h_surface});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : aeolian_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil, g.h_surface});
    }

    export_dune_data("test_dune.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Aeolian sand dune simulation exported to test_dune.js" << std::endl;
    return 0;
}
