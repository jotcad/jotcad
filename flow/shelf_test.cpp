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
        for (int y = 0; y < grid_size; y += 2) { // Downsample grid resolution slightly for output compression (128x128)
            out << "        [";
            for (int x = 0; x < grid_size; x += 2) {
                out << history[s].H_soil[y][x];
                if (x < grid_size - 2) out << ", ";
            }
            out << "]";
            if (y < grid_size - 2) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_h_surface: [\n";
        for (int y = 0; y < grid_size; y += 2) {
            out << "        [";
            for (int x = 0; x < grid_size; x += 2) {
                out << history[s].h_surface[y][x];
                if (x < grid_size - 2) out << ", ";
            }
            out << "]";
            if (y < grid_size - 2) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_sediment: [\n";
        for (int y = 0; y < grid_size; y += 2) {
            out << "        [";
            for (int x = 0; x < grid_size; x += 2) {
                out << history[s].sediment[y][x];
                if (x < grid_size - 2) out << ", ";
            }
            out << "]";
            if (y < grid_size - 2) out << ",";
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
    const int GRID_SIZE = 256; // High resolution grid for a whole continent
    Orchestrator orch(GRID_SIZE);
    Grid& g = orch.get_grid();

    // 1. Generate the radial continent and surrounding shelf
    std::cout << "Generating radial continental shelf topography (GRID_SIZE = 256)..." << std::endl;
    // Coast radius = 0.35, break radius = 0.60, abyssal start = 0.82
    ShelfGenerator::generate(g, 0.35f, 0.60f, 0.82f, 0.15f, -0.60f, -4.50f);

    // 2. Setup Phase 1: Subaerial River Runoff (no initial ocean standing water, high dt)
    const int steps_subaerial = 60;
    float dt_subaerial = 0.35f;
    Phase* p1 = orch.add_phase("Subaerial Runoff", dt_subaerial, steps_subaerial);
    p1->add<Precipitation>(0.16f);
    p1->add<Hydrodynamics>(9.81f, 0.018f);
    p1->add<Erosion>(0.15f, 0.04f, 0.18f, 0.20f, 0.06f, 0.08f, 0.40f);
    p1->add<Landslide>(0.48f, 255, 255);

    // Setup Phase 2: Marine Inundation (standing ocean, low dt respecting CFL)
    const int steps_marine = 240;
    float dt_marine = 0.05f;
    Phase* p2 = orch.add_phase("Marine Inundation", dt_marine, steps_marine);
    p2->add<Precipitation>(0.16f);
    p2->add<Hydrodynamics>(9.81f, 0.018f);
    p2->add<Erosion>(0.15f, 0.04f, 0.18f, 0.20f, 0.06f, 0.08f, 0.40f);
    p2->add<Landslide>(0.48f, 255, 255);

    // Pre-allocate required fields
    for (auto& element : p1->elements) {
        for (std::type_index type : element->get_required_fields()) g.request_field_by_type_index(type);
    }
    for (auto& element : p2->elements) {
        for (std::type_index type : element->get_required_fields()) g.request_field_by_type_index(type);
    }

    std::vector<ShelfState> history;

    auto record_history_state = [&](int global_step) {
        // Subsample global steps to keep exported JS file compact (every 10th step)
        if (global_step % 10 == 0 || global_step == steps_subaerial + steps_marine) {
            history.push_back({
                global_step,
                g.H_soil,
                g.h_surface,
                g.sediment
            });
        }
    };

    record_history_state(0);

    // 4. Run Phase 1: Subaerial Runoff
    std::cout << "Running Phase 1: Subaerial River Runoff..." << std::endl;
    for (int step = 0; step < steps_subaerial; ++step) {
        for (auto& element : p1->elements) {
            element->step(g, dt_subaerial, step, steps_subaerial);
        }
        record_history_state(step + 1);
    }

    // 5. Inundation: Flood the ocean basin to SEA_LEVEL = 0.0 before Phase 2 starts
    std::cout << "Flooding ocean basin to sea level z = 0.0..." << std::endl;
    const float SEA_LEVEL = 0.0f;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float water_needed = std::max(0.0f, SEA_LEVEL - g.H_soil[y][x]);
            // Merge standing ocean with any existing puddle/river water
            g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
        }
    }

    // 6. Run Phase 2: Marine Inundation
    std::cout << "Running Phase 2: Marine Inundation & Progradation..." << std::endl;
    for (int step = 0; step < steps_marine; ++step) {
        for (auto& element : p2->elements) {
            element->step(g, dt_marine, step, steps_marine);
        }
        record_history_state(steps_subaerial + step + 1);
    }

    // Export dataset
    std::cout << "Exporting shelf dataset to test_shelf.js..." << std::endl;
    export_shelf_data("test_shelf.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
