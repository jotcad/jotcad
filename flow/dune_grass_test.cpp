#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "vegetation.h"

struct DuneGrassState {
    int step;
    std::string phase_name;
    std::vector<std::vector<float>> H_soil;    // Bedrock
    std::vector<std::vector<float>> h_surface; // Active fluid (water depth or sand)
    std::vector<std::vector<float>> grass;     // Grass cover
    std::vector<std::vector<float>> tree;      // Tree cover
};

void export_dune_grass_data(const std::string& filename, const std::vector<DuneGrassState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const duneGrassData = {\n";
    out << "  grid_size: " << grid_size << ",\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        out << "      phase: \"" << history[s].phase_name << "\",\n";
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
        out << "      grid_grass: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].grass[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      grid_tree: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].tree[y][x];
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

    // 1. Initialize hilly bedrock topography with a central drainage saddle and rocky knobs
    PerlinNoise2D perlin;
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            float pn = perlin.noise(x * 0.08f, y * 0.08f) + 0.35f * perlin.noise(x * 0.22f, y * 0.22f);
            g.H_soil[y][x] = pn * 2.80f + 0.30f;

            // Rocky Knobs to deflect wind and concentrate channel flows
            float dx1 = (float)x - 30.0f;
            float dy1 = (float)y - 32.0f;
            float r1 = std::sqrt(dx1*dx1 + dy1*dy1);
            if (r1 < 8.0f) {
                g.H_soil[y][x] += 1.80f * (1.0f - r1 / 8.0f);
            }
            
            float dx2 = (float)x - 52.0f;
            float dy2 = (float)y - 48.0f;
            float r2 = std::sqrt(dx2*dx2 + dy2*dy2);
            if (r2 < 8.0f) {
                g.H_soil[y][x] += 1.80f * (1.0f - r2 / 8.0f);
            }
        }
    }

    // 2. Setup Phase 1: Vegetated Eco-Hydrology (Water Flow + Erosion + Landslides + Vegetation Growth)
    const int steps_hydro = 60;
    float dt_hydro = 0.40f;
    Phase* p1 = orch.add_phase("Eco-Hydrology", dt_hydro, steps_hydro);
    p1->add<Precipitation>(0.14f);
    p1->add<Hydrodynamics>(9.81f, 0.015f);
    // Erosion: erodibility = 0.12, friction = 0.18, settle = 0.16, infiltration = 0.12
    p1->add<Erosion>(0.12f, 0.05f, 0.18f, 0.16f, 0.12f, 0.08f, 0.40f);
    p1->add<Landslide>(0.58f, 74, 49); // landslide slope = 30 degrees (tan = 0.58)
    p1->add<Vegetation>(0.24f, 0.07f, 1.0f); // Grass grows, stabilizing banks

    // 3. Setup Phase 2: Vegetated Dunes (Aeolian Sand Dunes + Vegetation Growth)
    const int steps_aeolian = 60;
    float dt_aeolian = 0.40f;
    Phase* p2 = orch.add_phase("Vegetated Dunes", dt_aeolian, steps_aeolian);
    // Sand carrying Q0 = 0.50, repose = 32 degrees (0.62), relaxation = 1.60
    p2->add<AeolianDunes>(0.22f, 0.50f, 12.0f, 0.62f, 1.60f);
    p2->add<Vegetation>(0.18f, 0.06f, 1.0f); // Vegetation competes with dune burial

    // 4. Run the Orchestrated Simulation and collect history state
    std::vector<DuneGrassState> history;

    // Helper lambda to capture current grid state
    auto record_history_state = [&](int step_idx, const std::string& phase_name) {
        std::vector<std::vector<float>> grass_data(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        std::vector<std::vector<float>> tree_data(GRID_SIZE, std::vector<float>(GRID_SIZE, 0.0f));
        
        if (g.has_field<GrassField>()) grass_data = g.request_field<GrassField>();
        if (g.has_field<TreeField>()) tree_data = g.request_field<TreeField>();

        history.push_back({
            step_idx,
            phase_name,
            g.H_soil,
            g.h_surface,
            grass_data,
            tree_data
        });
    };

    // Record initial step
    record_history_state(0, "Eco-Hydrology");

    // Execute Phase 1: Hydrology
    std::cout << "--- Starting Phase 1: Eco-Hydrology ---" << std::endl;
    for (auto& element : p1->elements) {
        for (std::type_index type : element->get_required_fields()) g.request_field_by_type_index(type);
    }
    for (int step = 0; step < steps_hydro; ++step) {
        for (auto& element : p1->elements) {
            element->step(g, dt_hydro, step, steps_hydro);
        }
        record_history_state(step + 1, "Eco-Hydrology");
    }

    // Reset surface active layer from water to sand sheet for the Aeolian phase
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            g.h_surface[y][x] = 0.15f; // Initial uniform sand sheet thickness
        }
    }
    // Allocate sand field for Aeolian phase
    auto& sand_field = g.request_field<SandField>();
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            sand_field[y][x] = 0.15f;
        }
    }

    // Execute Phase 2: Vegetated Dunes
    std::cout << "--- Starting Phase 2: Vegetated Dunes ---" << std::endl;
    for (auto& element : p2->elements) {
        for (std::type_index type : element->get_required_fields()) g.request_field_by_type_index(type);
    }
    for (int step = 0; step < steps_aeolian; ++step) {
        for (auto& element : p2->elements) {
            element->step(g, dt_aeolian, step, steps_aeolian);
        }
        record_history_state(steps_hydro + step + 1, "Vegetated Dunes");
    }

    // Export dataset
    std::cout << "Exporting coupled simulation dataset to test_dune_grass_v2.js..." << std::endl;
    export_dune_grass_data("test_dune_grass_v2.js", history, GRID_SIZE);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
