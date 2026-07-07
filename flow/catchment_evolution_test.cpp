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

struct CatchmentState {
    int step;
    std::string phase_name;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> grass;
    std::vector<std::vector<float>> tree;
    std::vector<std::vector<float>> vx;
    std::vector<std::vector<float>> vy;
};

void export_catchment_data(const std::string& filename, const std::vector<CatchmentState>& history, int grid_size) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(2);
    out << "export const catchmentData = {\n";
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
        out << "      ],\n";

        // Export Velocity VX
        out << "      grid_vx: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].vx[y][x];
                if (x < grid_size - 1) out << ", ";
            }
            out << "]";
            if (y < grid_size - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";

        // Export Velocity VY
        out << "      grid_vy: [\n";
        for (int y = 0; y < grid_size; ++y) {
            out << "        [";
            for (int x = 0; x < grid_size; ++x) {
                out << history[s].vy[y][x];
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

class CatchmentPrecipitation : public Element {
private:
    float base_rate;
    float mountain_rate;
public:
    CatchmentPrecipitation(float base, float mountain) : base_rate(base), mountain_rate(mountain) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        float catchment_radius = g.size * 0.40f;
        float base_meters = base_rate / 1000.0f;
        float mountain_meters = mountain_rate / 1000.0f;

        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float dist_to_nw = std::sqrt((float)(x * x + y * y));
                if (dist_to_nw < catchment_radius) {
                    g.h_surface[y][x] += mountain_meters * dt;
                } else {
                    g.h_surface[y][x] += base_meters * dt;
                }
            }
        }
    }
};

int main() {
    const int GRID_LOW = 256;
    const int GRID_HIGH = 512;
    Orchestrator orch(GRID_LOW);
    Grid& g = orch.get_grid();

    // 1. Initialize custom flat plain with northwest mountain catchment
    std::cout << "Initializing custom flat plain & mountain catchment (256x256)..." << std::endl;
    int sz = g.size;
    PerlinNoise2D perlin;
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            // 1. Base plain sloping gently from northwest to southeast
            // (0,0) is northwest, (sz, sz) is southeast
            float slope_t = (float)(x + y) / (2.0f * sz);
            float z = 0.55f * (1.0f - slope_t) + 0.02f * slope_t; 

            // 2. Gentle diagonal steering valley across the entire grid (mountain canyon + plains channel)
            float d = std::abs((float)(y - x)) / 1.414f;
            float valley_width = sz * 0.08f; // Valley width scales with grid size
            
            float base_valley_depth = 0.15f;
            float dist_to_nw = std::sqrt((float)(x * x + y * y));
            float catchment_radius = sz * 0.40f; // Covers northwest 40% of grid size
            
            float valley_scale = 1.0f;
            if (dist_to_nw < catchment_radius) {
                float t = dist_to_nw / catchment_radius;
                valley_scale = 1.0f + 6.0f * (1.0f - t * t); // Up to 7x deeper in high mountains (canyon)
            }
            float valley_dep = -base_valley_depth * valley_scale * std::exp(-(d * d) / (valley_width * valley_width));
            z += valley_dep;

            // 3. Northwest mountain catchment (semi-circular block around (0, 0))
            if (dist_to_nw < catchment_radius) {
                float t = dist_to_nw / catchment_radius;
                // Mountains rising up to 8.5 meters
                float mountain_z = 8.50f * (1.0f - t * t);
                
                // Scale down the ruggedness noise near the valley center to keep the canyon bed clear
                float noise_attenuation = 1.0f - 0.85f * std::exp(-(d * d) / (valley_width * valley_width));
                float ruggedness = 1.20f * perlin.noise(x * 0.08f, y * 0.08f) +
                                   0.30f * perlin.noise(x * 0.25f, y * 0.25f);
                z += mountain_z + ruggedness * noise_attenuation;
            } else {
                // Add small plain roughness (8 cm)
                z += perlin.noise(x * 0.05f, y * 0.05f) * 0.08f;
            }

            // Define the ocean basin in the far southeast corner (where slope_t > 0.85)
            if (slope_t > 0.85f) {
                g.H_soil[y][x] = -4.50f;
            } else {
                g.H_soil[y][x] = z;
            }
        }
    }

    g.request_field<GrassField>();
    g.request_field<TreeField>();
    auto& soil = g.request_field<SoilField>();

    // Initialize soil cover (0.20m on subaerial land, 0.02m on seabed)
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            soil[y][x] = (g.H_soil[y][x] > 0.0f) ? 0.20f : 0.02f;
        }
    }

    // 2. Setup Phase 1: Catchment Setup (Quick initial setup phase)
    const int steps_tectonic = 5;
    float dt_tectonic = 1.0f;
    Phase* p1 = orch.add_phase("Catchment Setup", dt_tectonic, steps_tectonic, GRID_LOW);
    p1->add<Precipitation>(0.0f); // No rain yet

    // 3. Setup Phase 1.5: Micro-Terrain Noise (Runs on upscaled 512x512 grid immediately after rescaling)
    const int steps_noise = 1;
    float dt_noise = 1.0f;
    Phase* p_noise = orch.add_phase("Micro-Terrain Noise", dt_noise, steps_noise, GRID_HIGH);
    p_noise->add<NoiseElement>(0.45f, 0.05f, false); // Add 5 cm Perlin noise directly once

    // 4. Setup Phase 2: Hydrological Incision (Runs on upscaled 512x512 grid)
    const int steps_hydro = 120;
    float dt_hydro = 0.05f;
    Phase* p2 = orch.add_phase("Hydrology & Incision", dt_hydro, steps_hydro, GRID_HIGH);
    p2->add<CatchmentPrecipitation>(0.10f, 6.5f); // High rain in northwest mountain catchment
    p2->add<Hydrodynamics>(9.81f, 0.018f);
    p2->add<Erosion>(0.15f, 0.04f, 0.18f, 0.0f, 0.012f, 0.08f, 0.40f, false); // Zero settling rate to prevent all deposition dams

    // 5. Setup Phase 3: Coupled Eco-Vegetation (Runs on upscaled 512x512 grid)
    const int steps_eco = 5000;
    float dt_eco = 0.05f;
    Phase* p3 = orch.add_phase("Eco-Vegetation Growth", dt_eco, steps_eco, GRID_HIGH);
    p3->add<CatchmentPrecipitation>(0.15f, 5.0f); // Moderate rain on plains, high rain in catchment
    p3->add<Hydrodynamics>(9.81f, 0.018f);
    p3->add<Erosion>(0.15f, 0.04f, 0.18f, 0.0f, 0.0f, 0.0f, 0.40f, false); // Zero infiltration and lateral diffusion to prevent surface water loss
    p3->add<Landslide>(0.55f, 511, 511); // Realistic soil/rock repose angle (0.55) to prevent mountain valley burial
    p3->add<Vegetation>(2.20f, 0.0f, 1.0f); // Dynamic growth (trees disabled)

    std::vector<CatchmentState> history;

    // Helper to capture active simulation state and downsample if resolution is high
    auto record_history_state = [&](int global_step, const std::string& phase_name) {
        if (global_step % 50 == 0 || global_step == steps_tectonic + steps_noise + steps_hydro + steps_eco) {
            const int export_size = 128;
            std::vector<std::vector<float>> H_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> h_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> grass_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> tree_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> vx_down(export_size, std::vector<float>(export_size));
            std::vector<std::vector<float>> vy_down(export_size, std::vector<float>(export_size));

            auto& grass = g.request_field<GrassField>();
            auto& tree = g.request_field<TreeField>();

            int stride = g.size / export_size;
            for (int y = 0; y < export_size; ++y) {
                for (int x = 0; x < export_size; ++x) {
                    H_down[y][x] = g.H_soil[y * stride][x * stride];
                    h_down[y][x] = g.h_surface[y * stride][x * stride];
                    grass_down[y][x] = grass[y * stride][x * stride];
                    tree_down[y][x] = tree[y * stride][x * stride];
                    // Average the staggered velocities to the cell center for visualization
                    vx_down[y][x] = 0.5f * (g.vx[x * stride][y * stride] + g.vx[x * stride + 1][y * stride]);
                    vy_down[y][x] = 0.5f * (g.vy[x * stride][y * stride] + g.vy[x * stride][y * stride + 1]);
                }
            }

            history.push_back({
                global_step,
                phase_name,
                H_down,
                h_down,
                grass_down,
                tree_down,
                vx_down,
                vy_down
            });
        }
    };

    // Capture initial state
    record_history_state(0, "Catchment Base");

    // Run Phase 1
    std::cout << "Running Phase 1: Catchment Setup (256x256)..." << std::endl;
    if (p1->target_grid_size > 0 && g.size != p1->target_grid_size) {
        g.upscale_in_place(p1->target_grid_size);
    }
    for (int step = 0; step < steps_tectonic; ++step) {
        for (auto& element : p1->elements) {
            element->step(g, dt_tectonic, step, steps_tectonic);
        }
        record_history_state(step + 1, "Catchment Setup");
    }

    // Run Phase 1.5: Micro-Terrain Noise
    std::cout << "Running Phase: Micro-Terrain Noise (GRID_HIGH)..." << std::endl;
    if (p_noise->target_grid_size > 0 && g.size != p_noise->target_grid_size) {
        std::cout << "Transitioning Phase: Rescaling grid to " << p_noise->target_grid_size << "..." << std::endl;
        g.upscale_in_place(p_noise->target_grid_size);
    }
    for (int step = 0; step < steps_noise; ++step) {
        for (auto& element : p_noise->elements) {
            element->step(g, dt_noise, step, steps_noise);
        }
        record_history_state(steps_tectonic + step + 1, "Micro-Terrain Noise");
    }

    // Flooding ocean basin to sea level z = 0.0 before hydrology
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            if (g.H_soil[y][x] <= 0.0f) {
                g.h_surface[y][x] = -g.H_soil[y][x];
            } else {
                g.h_surface[y][x] = 0.0f;
            }
        }
    }

    // Run Phase 2: Hydrology & River Incision
    std::cout << "Running Phase 2: Hydrology & River Incision (512x512)..." << std::endl;
    if (p2->target_grid_size > 0 && g.size != p2->target_grid_size) {
        g.upscale_in_place(p2->target_grid_size);
    }
    for (int step = 0; step < steps_hydro; ++step) {
        for (auto& element : p2->elements) {
            element->step(g, dt_hydro, step, steps_hydro);
        }
        
        // Ocean level boundary condition (reset ocean level to z = 0)
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                if (g.H_soil[y][x] <= 0.0f) {
                    g.h_surface[y][x] = -g.H_soil[y][x];
                }
            }
        }
        record_history_state(steps_tectonic + steps_noise + step + 1, "Hydrology & Incision");
    }

    // Run Phase 3: Eco-Vegetation Growth
    std::cout << "Running Phase 3: Eco-Vegetation Growth (512x512)..." << std::endl;
    if (p3->target_grid_size > 0 && g.size != p3->target_grid_size) {
        g.upscale_in_place(p3->target_grid_size);
    }
    
    // Seed initial vegetation along mountain valleys where moisture starts to aggregate
    auto& grass = g.request_field<GrassField>();
    auto& tree = g.request_field<TreeField>();
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            // Seed only on dry land cells (not inside the active river channel)
            if (g.H_soil[y][x] > 0.0f && g.H_soil[y][x] < 4.0f && g.h_surface[y][x] <= 0.002f) {
                grass[y][x] = 0.15f;
                tree[y][x] = 0.0f; // Trees disabled
            }
        }
    }

    for (int step = 0; step < steps_eco; ++step) {
        for (auto& element : p3->elements) {
            element->step(g, dt_eco, step, steps_eco);
        }
        
        // Ocean level boundary condition (reset ocean level to z = 0)
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                if (g.H_soil[y][x] <= 0.0f) {
                    g.h_surface[y][x] = -g.H_soil[y][x];
                }
            }
        }
        record_history_state(steps_tectonic + steps_noise + steps_hydro + step + 1, "Eco-Vegetation Growth");
    }

    std::cout << "Exporting catchment evolution dataset to test_catchment.js..." << std::endl;
    export_catchment_data("test_catchment.js", history, GRID_LOW / 2);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
