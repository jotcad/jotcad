#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include "flow_solver.h"
#include "shelf_generator.h"
#include "noise.h"

struct ContinentState {
    int step;
    std::string phase_name;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> grass;
    std::vector<std::vector<float>> tree;
};

void export_continent_data(const std::string& filename, const std::vector<ContinentState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const continentData = {\n";
    out << "  grid_size: " << grid_size << ",\n";
    out << "  steps: [\n";
    for (size_t s = 0; s < history.size(); ++s) {
        out << "    {\n";
        out << "      step: " << history[s].step << ",\n";
        out << "      phase: \"" << history[s].phase_name << "\",\n";
        
        // Export Bedrock Heights
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

        // Export Water Surface Heights
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

        // Export Grass Density
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

        // Export Tree Density
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
    // Start Phase 1 on 512x512 grid (Doubled grid dimensions)
    const int GRID_LOW = 512;
    const int GRID_HIGH = 1024;
    Orchestrator orch(GRID_LOW);
    Grid& g = orch.get_grid();

    // 1. Initialize the continent profile (512x512)
    std::cout << "Initializing irregular continental basement topography (512x512)..." << std::endl;
    // Broad shelf (break_u - coast_u = 0.27), high boundary noise (0.40), and high mountain ruggedness (0.90) with unscaled height (4.00m)
    ShelfGenerator::generate(g, 0.65f, 0.92f, 0.98f, 0.15f, -0.50f, -4.50f, 4.00f, 0.40f, 0.90f);

    // Pre-request vegetation and soil fields in low resolution
    g.request_field<GrassField>();
    g.request_field<TreeField>();
    auto& soil = g.request_field<SoilField>();

    // Initialize soil cover (0.20m on subaerial land, 0.02m on seabed)
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            soil[y][x] = (g.H_soil[y][x] > 0.0f) ? 0.20f : 0.02f;
        }
    }

    // 2. Setup Phase 1: Bolide Impact (Excavates a large central crater lake depression and rebound peak)
    const int steps_tectonic = 60;
    float dt_tectonic = 1.0f;
    // Configure Phase 1 with target grid size GRID_LOW (512)
    Phase* p1 = orch.add_phase("Bolide Impact", dt_tectonic, steps_tectonic, GRID_LOW);
    p1->add<ImpactCrater>(256.0f, 256.0f, 70.0f, 4.0f, 2.0f); // Crater center at 256,256 and radius 70 (scaled to doubled grid)

    // 3. Setup Phase 1.5: Micro-Terrain Noise (Runs on upscaled 1024x1024 grid immediately after rescaling)
    const int steps_noise = 1;
    float dt_noise = 1.0f;
    Phase* p_noise = orch.add_phase("Micro-Terrain Noise", dt_noise, steps_noise, GRID_HIGH);
    p_noise->add<NoiseElement>(0.45f, 0.05f, false); // Add 5 cm Perlin noise directly once

    // 4. Setup Phase 2: Hydrological Erosion & Incision (Runs on upscaled 512x512 grid)
    const int steps_hydro = 120;
    float dt_hydro = 0.05f;
    // Configure Phase 2 with target grid size GRID_HIGH (512)
    Phase* p2 = orch.add_phase("Hydrology & Incision", dt_hydro, steps_hydro, GRID_HIGH);
    p2->add<Precipitation>(6.0f, true); // Balanced scale-invariant precipitation
    p2->add<Hydrodynamics>(9.81f, 0.018f);
    p2->add<Erosion>(0.15f, 0.04f, 0.18f, 0.20f, 0.06f, 0.08f, 0.40f);

    // 4. Setup Phase 3: Coupled Eco-Vegetation (Runs on upscaled 1024x1024 grid)
    const int steps_eco = 2000;
    float dt_eco = 0.05f;
    // Configure Phase 3 with target grid size GRID_HIGH (1024)
    Phase* p3 = orch.add_phase("Eco-Vegetation Growth", dt_eco, steps_eco, GRID_HIGH);
    p3->add<Precipitation>(2.0f, true);
    p3->add<Hydrodynamics>(9.81f, 0.018f);
    p3->add<Erosion>(0.15f, 0.04f, 0.18f, 0.20f, 0.06f, 0.08f, 0.40f);
    p3->add<Landslide>(0.16f, 1023, 1023); // Lower repose angle, boundary sink at 1023,1023
    p3->add<Vegetation>(2.20f, 0.70f, 1.0f); // Dynamic growth
 
    std::vector<ContinentState> history;
 
    // Helper to capture active simulation state and downsample if resolution is high
    auto record_history_state = [&](int global_step, const std::string& phase_name) {
        if (global_step % 50 == 0 || global_step == steps_tectonic + steps_noise + steps_hydro + steps_eco) {
            const int export_size = 256; // Doubled export resolution
            std::vector<std::vector<float>> H_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> h_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> grass_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> tree_down(export_size, std::vector<float>(export_size));
 
            auto& grass = g.request_field<GrassField>();
            auto& tree = g.request_field<TreeField>();
 
            // Dynamic downsampling based on simulation-to-export stride ratio
            int stride = g.size / export_size;
            for (int y = 0; y < export_size; ++y) {
                for (int x = 0; x < export_size; ++x) {
                    H_down[y][x] = g.H_soil[y * stride][x * stride];
                    h_down[y][x] = g.h_surface[y * stride][x * stride];
                    grass_down[y][x] = grass[y * stride][x * stride];
                    tree_down[y][x] = tree[y * stride][x * stride];
                }
            }

            history.push_back({
                global_step,
                phase_name,
                H_down,
                h_down,
                grass_down,
                tree_down
            });
        }
    };

    // Capture initial state
    record_history_state(0, "Continental Base");

    // Run Phase 1: Bolide Impact (512x512)
    std::cout << "Running Phase 1: Bolide Impact Excavation (512x512)..." << std::endl;
    // Enforce target size from phase config
    if (p1->target_grid_size > 0 && g.size != p1->target_grid_size) {
        g.upscale_in_place(p1->target_grid_size);
    }
    for (int step = 0; step < steps_tectonic; ++step) {
        for (auto& element : p1->elements) {
            element->step(g, dt_tectonic, step, steps_tectonic);
        }
        record_history_state(step + 1, "Bolide Impact");
    }

    // Run Phase 1.5: Micro-Terrain Noise (Rescales grid and applies Perlin noise)
    std::cout << "Running Phase: Micro-Terrain Noise (GRID_HIGH)..." << std::endl;
    if (p_noise->target_grid_size > 0 && g.size != p_noise->target_grid_size) {
        std::cout << "Transitioning Phase: Rescaling grid to " << p_noise->target_grid_size 
                  << " as specified by phase '" << p_noise->name << "' configuration..." << std::endl;
        g.upscale_in_place(p_noise->target_grid_size);
    }
    for (int step = 0; step < steps_noise; ++step) {
        for (auto& element : p_noise->elements) {
            element->step(g, dt_noise, step, steps_noise);
        }
        record_history_state(steps_tectonic + step + 1, "Micro-Terrain Noise");
    }

    // Inundation Transition: Flood ocean basin to sea level z = 0.0 (512x512)
    std::cout << "Flooding ocean basin to sea level z = 0.0 before hydrology..." << std::endl;
    const float SEA_LEVEL = 0.0f;
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            float water_needed = std::max(0.0f, SEA_LEVEL - g.H_soil[y][x]);
            g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
        }
    }

    // Run Phase 2: Hydrological Incision (512x512)
    std::cout << "Running Phase 2: Hydrology & River Incision (512x512)..." << std::endl;
    for (int step = 0; step < steps_hydro; ++step) {
        for (auto& element : p2->elements) {
            element->step(g, dt_hydro, step, steps_hydro);
        }
        // Enforce Global Ocean Level Boundary Condition: replenish evaporated/infiltrated ocean water
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float water_needed = std::max(0.0f, 0.0f - g.H_soil[y][x]);
                g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
            }
        }
        record_history_state(steps_tectonic + steps_noise + step + 1, "Hydrology & Incision");
    }
 
    // Run Phase 3: Coupled Eco-Vegetation (1024x1024)
    std::cout << "Running Phase 3: Eco-Vegetation Growth (1024x1024)..." << std::endl;
    if (p3->target_grid_size > 0 && g.size != p3->target_grid_size) {
        g.upscale_in_place(p3->target_grid_size);
    }
    // Add fine-grain sub-grid Perlin noise pass immediately at the start of Phase 3
    std::cout << "Adding fine-grain sub-grid noise pass for vegetation colonization..." << std::endl;
    {
        PerlinNoise2D perlin;
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                // High frequency, low amplitude micro-roughness (approx. 3.5 cm peaks)
                float noise_val = perlin.noise(x * 0.45f, y * 0.45f);
                g.H_soil[y][x] += noise_val * 0.035f;
            }
        }
    }
    for (int step = 0; step < steps_eco; ++step) {
        for (auto& element : p3->elements) {
            element->step(g, dt_eco, step, steps_eco);
        }
        // Enforce Global Ocean Level Boundary Condition: replenish evaporated/infiltrated ocean water
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float water_needed = std::max(0.0f, 0.0f - g.H_soil[y][x]);
                g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
            }
        }
        record_history_state(steps_tectonic + steps_noise + steps_hydro + step + 1, "Eco-Vegetation Growth");
    }

    // Export dataset
    std::cout << "Exporting continental evolution dataset to test_continent_large.js..." << std::endl;
    export_continent_data("test_continent_large.js", history, 256);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
