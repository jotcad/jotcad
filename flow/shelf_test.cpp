#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "shelf_generator.h"

struct ShelfState {
    int step;
    std::vector<std::vector<float>> H_soil;    // Bedrock
    std::vector<std::vector<float>> h_surface; // Water depth
    std::vector<std::vector<float>> sediment;  // Suspended sediment
};

void export_shelf_data(const std::string& filename, const std::vector<ShelfState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const shelfData = {\n";
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
        out << "      ],\n";
        out << "      grid_sediment: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].sediment[y][x];
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
    Orchestrator orch(GRID_SIZE);
    Grid& g = orch.get_grid();

    // 1. Generate the passive margin continental shelf
    std::cout << "Generating continental shelf topography..." << std::endl;
    // Shoreline at x = 20 (0.25), Break at x = 48 (0.60), Abyssal Plain at x = 68 (0.85)
    ShelfGenerator::generate(g, 0.25f, 0.60f, 0.85f, 0.20f, -0.60f, -4.50f);

    // 2. Initialize sea water level to z = 0.0 (standing ocean)
    const float SEA_LEVEL = 0.0f;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            g.h_surface[y][x] = std::max(0.0f, SEA_LEVEL - g.H_soil[y][x]);
        }
    }

    // 3. Setup Phase: Marine Progradation (Hydrology + Erosion + Landslides)
    const int steps = 100;
    float dt = 0.35f;
    Phase* p = orch.add_phase("Shelf Sedimentation", dt, steps);
    
    // Rain discharges primarily on landward side (West half)
    p->add<Precipitation>(0.15f);
    p->add<Hydrodynamics>(9.81f, 0.015f);
    // Erosion parameters: erodibility = 0.16, settle = 0.18, infiltration = 0.05
    p->add<Erosion>(0.16f, 0.04f, 0.18f, 0.18f, 0.05f, 0.08f, 0.40f);
    p->add<Landslide>(0.45f, 79, 79); // steeper landslides to model subaqueous slope collapses

    // Pre-allocate sparse fields if any elements request them (none here, but good practice)
    for (auto& element : p->elements) {
        for (std::type_index type : element->get_required_fields()) g.request_field_by_type_index(type);
    }

    std::vector<ShelfState> history;

    auto record_history_state = [&](int step_idx) {
        history.push_back({
            step_idx,
            g.H_soil,
            g.h_surface,
            g.sediment
        });
    };

    record_history_state(0);

    // 4. Execute the step loop, enforcing the ocean standing sea level boundary condition
    std::cout << "Running Marine Shelf progradation simulation..." << std::endl;
    for (int step = 0; step < steps; ++step) {
        // Run physics elements
        for (auto& element : p->elements) {
            element->step(g, dt, step, steps);
        }

        // Apply Ocean Sea Level Boundary Condition
        // Eastern half is a large standing reservoir at SEA_LEVEL = 0.0
        // We clamp the water height and reset boundary velocity to simulate open ocean
        int break_x_idx = (int)(0.50f * GRID_SIZE);
        for (int y = 0; y < GRID_SIZE; ++y) {
            for (int x = break_x_idx; x < GRID_SIZE; ++x) {
                g.h_surface[y][x] = std::max(0.0f, SEA_LEVEL - g.H_soil[y][x]);
                
                // Clear fluid momentum at ocean boundary using correct staggered [x][y] indexing
                g.vx[x][y] = 0.0f;
                if (x + 1 <= GRID_SIZE) g.vx[x+1][y] = 0.0f;
                g.vy[x][y] = 0.0f;
                if (y + 1 <= GRID_SIZE) g.vy[x][y+1] = 0.0f;
            }
        }

        record_history_state(step + 1);
    }

    // Export dataset
    std::cout << "Exporting shelf dataset to test_shelf.js..." << std::endl;
    export_shelf_data("test_shelf.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
