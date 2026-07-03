#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "glacier.h"

struct GlacialState {
    int step;
    std::vector<std::vector<float>> H_soil;    // Bedrock knobs
    std::vector<std::vector<float>> h_surface; // Till depth
};

void export_glacial_data(const std::string& filename, const std::vector<GlacialState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const glacialData = {\n";
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

    // Initial flat terrain with three rocky knobs to trigger drumlin plastering downstream
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            g.H_soil[y][x] = 0.5f;

            // Knob 1 (North-West)
            float dx1 = (float)x - 22.0f;
            float dy1 = (float)y - 25.0f;
            float r1 = std::sqrt(dx1*dx1 + dy1*dy1);
            if (r1 < 6.0f) {
                g.H_soil[y][x] += 1.8f * (1.0f - r1 / 6.0f);
            }

            // Knob 2 (Center)
            float dx2 = (float)x - 42.0f;
            float dy2 = (float)y - 40.0f;
            float r2 = std::sqrt(dx2*dx2 + dy2*dy2);
            if (r2 < 7.0f) {
                g.H_soil[y][x] += 2.0f * (1.0f - r2 / 7.0f);
            }

            // Knob 3 (South-East)
            float dx3 = (float)x - 58.0f;
            float dy3 = (float)y - 55.0f;
            float r3 = std::sqrt(dx3*dx3 + dy3*dy3);
            if (r3 < 6.5f) {
                g.H_soil[y][x] += 1.9f * (1.0f - r3 / 6.5f);
            }
        }
    }

    const int total_steps = 120;
    float dt = 0.4f; // step scale

    Phase glacial_phase("Glacial Deposition", dt, total_steps);
    // vx = 0.8, vy = 0.8 (diagonal NW to SE movement)
    // dep = 0.18, er = 0.08, drag = 0.12
    glacial_phase.add<Glacier>(0.0f, 1.0f, 0.18f, 0.08f, 0.12f);

    std::vector<GlacialState> history;
    history.push_back({0, g.H_soil, g.h_surface});

    for (int step = 1; step <= total_steps; ++step) {
        for (auto& element : glacial_phase.elements) {
            element->step(g, dt, step, total_steps);
        }
        history.push_back({step, g.H_soil, g.h_surface});
    }

    export_glacial_data("test_glacial.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Glacial simulation exported to test_glacial.js" << std::endl;
    return 0;
}
