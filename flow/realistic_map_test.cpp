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

struct RealisticMapState {
    int step;
    std::string phase_name;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> grass;
    std::vector<std::vector<float>> tree;
    std::vector<std::vector<float>> vx;
    std::vector<std::vector<float>> vy;
};

void export_map_data(const std::string& filename, const std::vector<RealisticMapState>& history, int grid_size, float cell_spacing_m, float height_scale_m) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(3);
    out << "export const realisticMapData = {\n";
    out << "  grid_size: " << grid_size << ",\n";
    out << "  cell_spacing_m: " << cell_spacing_m << ",\n";
    out << "  height_scale_m: " << height_scale_m << ",\n";
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

class RealisticPrecipitation : public Element {
private:
    float base_rate;
    float mountain_rate;
    float alt_threshold;
public:
    RealisticPrecipitation(float base, float mountain, float threshold = 0.32f) 
        : base_rate(base), mountain_rate(mountain), alt_threshold(threshold) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        float base_meters = base_rate / 1000.0f;
        float mountain_meters = mountain_rate / 1000.0f;

        // River path meander parameters to match main
        float meander_amp = g.size * 0.12f;
        float meander_freq = 0.06f;

        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float H = g.H_soil[y][x];
                
                float target_x = (g.size - 1 - y);
                float meander_x = target_x + meander_amp * std::sin(y * meander_freq);
                float d_canyon = std::abs((float)(x - meander_x));

                // Restrict heavy rain strictly by altitude AND canyon corridor (unless alt_threshold is negative)
                bool rain_allowed = (alt_threshold < 0.0f) ? (d_canyon < g.size * 0.12f)
                                                           : (H > alt_threshold && d_canyon < g.size * 0.12f);
                if (rain_allowed) {
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
    g.scale.cell_spacing_m = 50.0f;  // 50 meters per cell at 256x256
    g.scale.height_scale_m = 500.0f; // 500 meters vertical unit

    std::cout << "Initializing realistic island topography (256x256)..." << std::endl;
    int sz = g.size;
    PerlinNoise2D perlin;
    float center_x = sz / 2.0f;
    float center_y = sz / 2.0f;
    float R_max = sz / 2.0f;

    // Define central mountain peak coordinates
    float mount_cx = center_x;
    float mount_cy = center_y;
    float mountain_w = sz * 0.06f; // Extremely compact mountain core

    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float r_dist = std::sqrt(dx*dx + dy*dy);
            
            // 1. Broad Organic Island Outline (expanded to 1.15f to make land twice as broad)
            float coast_u = 1.15f;
            float u_noise = perlin.noise(x * 0.035f, y * 0.035f) * 0.12f +
                            perlin.noise(x * 0.12f, y * 0.12f) * 0.04f;
            float u = r_dist / R_max + u_noise;

            if (u < coast_u) {
                // 2. Very flat plain base: only slopes from 0.08m to 0.02m (no steep doming)
                float land_t = u / coast_u;
                float z = 0.02f + 0.06f * std::cos(land_t * 1.5708f);

                // Add rolling hills using low-amplitude noise
                float hills = 0.08f * perlin.noise(x * 0.05f, y * 0.05f) +
                              0.02f * perlin.noise(x * 0.18f, y * 0.18f);
                z += hills;

                // 3. Central Mountain Peak modeled as a Lorentzian curve (extremely broad, smooth foot)
                float dist_to_mount = std::sqrt((x - mount_cx) * (x - mount_cx) + (y - mount_cy) * (y - mount_cy));
                float mountain_z = 2.80f / (1.0f + (dist_to_mount / mountain_w) * (dist_to_mount / mountain_w));
                
                // Add rugged mountain details scaled by mountain height (decays to 0 at the broad foot)
                float rugged_scale = mountain_z / 2.80f;
                float ruggedness = 0.65f * perlin.noise(x * 0.08f, y * 0.08f) +
                                   0.20f * perlin.noise(x * 0.25f, y * 0.25f);
                z += mountain_z + ruggedness * rugged_scale;

                // 4. Winding river valley starting from the central peak down to the southwest coast
                float target_x = (sz - 1 - y);
                float meander_amp = sz * 0.12f; // Sinuous amplitude
                float meander_freq = 0.06f;     // Sinuous frequency
                float meander_x = target_x + meander_amp * std::sin(y * meander_freq);
                
                float d_canyon = std::abs((float)(x - meander_x)) / 1.414f;
                float canyon_width = sz * 0.06f;
                
                // Gorge is deep near the peak, and transitions to a shallow channel on the plains
                float canyon_scale = 1.0f + 3.0f * rugged_scale;
                float base_canyon_depth = 0.18f; // Shallow plains channel
                float canyon_dep = -base_canyon_depth * canyon_scale * std::exp(-(d_canyon * d_canyon) / (canyon_width * canyon_width));
                z += canyon_dep;

                g.H_soil[y][x] = z;
            } else {
                // Ocean basin
                float ocean_depth = -4.50f * (1.0f - std::exp(-4.0f * (u - coast_u)));
                g.H_soil[y][x] = ocean_depth;
            }
        }
    }

    g.request_field<GrassField>();
    g.request_field<TreeField>();
    auto& soil = g.request_field<SoilField>();

    // Soil thickness setup (0.25m on land, 0.02m in ocean)
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            soil[y][x] = (g.H_soil[y][x] > 0.0f) ? 0.25f : 0.02f;
        }
    }

    // 1. Phase 1: Landscape Genesis
    const int steps_tectonic = 5;
    float dt_tectonic = 1.0f;
    Phase* p1 = orch.add_phase("Landscape Genesis", dt_tectonic, steps_tectonic, GRID_LOW);
    p1->add<Precipitation>(0.0f); // Setup only

    // 2. Phase 1.5: Micro-Terrain Noise
    const int steps_noise = 1;
    float dt_noise = 1.0f;
    Phase* p_noise = orch.add_phase("Micro-Terrain Noise", dt_noise, steps_noise, GRID_HIGH);
    p_noise->add<NoiseElement>(0.45f, 0.05f, false); // Add 5 cm Perlin noise

    // 3. Phase 2: Hydrological Incision (Cuts channels and shapes valleys)
    const int steps_hydro = 120;
    float dt_hydro = 0.05f;
    Phase* p2 = orch.add_phase("Hydrology & Incision", dt_hydro, steps_hydro, GRID_HIGH);
    p2->add<RealisticPrecipitation>(0.0f, 6.5f, -999.0f); // Enable altitude bypass during incision to carve canyon to coast
    p2->add<Hydrodynamics>(9.81f, 0.018f);
    // HACK: Set critical shear stress threshold (second argument) to 0.0f, and boost erodibility (first argument) to 500.0f.
    // Physically, this corrects the 10,000x normalized-to-physical shear stress scaling mismatch (tau = h * S vs 9810 * h * S)
    // and models the easily erodible, soft alluvial sand/silt sediments of the lowlands plain (tau_c_physical ~0.1 Pa).
    p2->add<Erosion>(500.0f, 0.0f, 0.18f, 0.0f, 0.0f, 0.0f, 0.45f, false); // Zero settling, zero infiltration

    // 4. Phase 3: Coupled Eco-Vegetation
    const int steps_eco = 2000;
    float dt_eco = 0.05f;
    Phase* p3 = orch.add_phase("Eco-Vegetation Growth", dt_eco, steps_eco, GRID_HIGH);
    p3->add<RealisticPrecipitation>(0.80f, 12.50f, 0.32f); // Plain rain at 0.80f (temperate plain) to drain lake while keeping river inside canyon
    p3->add<Hydrodynamics>(9.81f, 0.018f);
    // Restore standard Erosion parameters (0.0035f infiltration, 0.015f lateral Darcy diffusion)
    p3->add<Erosion>(0.15f, 0.04f, 0.18f, 0.0f, 0.0035f, 0.015f, 0.45f, false);
    p3->add<Landslide>(0.55f, 511, 511); // High repose slope to stabilize river canyon walls
    p3->add<Vegetation>(2.20f, 0.50f, 1.0f); // Dynamic growth (grass & trees enabled)

    std::vector<RealisticMapState> history;

    // Helper to capture active simulation state at native resolution (no downsampling)
    auto record_history_state = [&](int global_step, const std::string& phase_name) {
        if (global_step % 500 == 0 || global_step == steps_tectonic + steps_noise + steps_hydro + steps_eco) {
            const int export_size = 512;
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
                    vx_down[y][x] = g.vx[y * stride][x * stride];
                    vy_down[y][x] = g.vy[y * stride][x * stride];
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

    record_history_state(0, "Landscape Genesis");

    // Run Phase 1
    std::cout << "Running Phase 1: Landscape Genesis..." << std::endl;
    if (p1->target_grid_size > 0 && g.size != p1->target_grid_size) {
        g.upscale_in_place(p1->target_grid_size);
    }
    for (int step = 0; step < steps_tectonic; ++step) {
        for (auto& element : p1->elements) {
            element->step(g, dt_tectonic, step, steps_tectonic);
        }
        record_history_state(step + 1, "Landscape Genesis");
    }

    // Run Phase 1.5
    std::cout << "Running Phase: Micro-Terrain Noise..." << std::endl;
    if (p_noise->target_grid_size > 0 && g.size != p_noise->target_grid_size) {
        g.upscale_in_place(p_noise->target_grid_size);
    }
    for (int step = 0; step < steps_noise; ++step) {
        for (auto& element : p_noise->elements) {
            element->step(g, dt_noise, step, steps_noise);
        }
        record_history_state(steps_tectonic + step + 1, "Micro-Terrain Noise");
    }

    // Run Phase 2
    std::cout << "Running Phase 2: Hydrology & River Incision..." << std::endl;
    for (int step = 0; step < steps_hydro; ++step) {
        for (auto& element : p2->elements) {
            element->step(g, dt_hydro, step, steps_hydro);
        }
        // Replenish ocean water level (z = 0.0)
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float water_needed = std::max(0.0f, 0.0f - g.H_soil[y][x]);
                g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
            }
        }
        record_history_state(steps_tectonic + steps_noise + step + 1, "Hydrology & Incision");
    }

    // Run Phase 3
    std::cout << "Running Phase 3: Coupled Eco-Vegetation..." << std::endl;
    for (int step = 0; step < steps_eco; ++step) {
        for (auto& element : p3->elements) {
            element->step(g, dt_eco, step, steps_eco);
        }
        // Replenish ocean water level (z = 0.0)
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float water_needed = std::max(0.0f, 0.0f - g.H_soil[y][x]);
                g.h_surface[y][x] = std::max(g.h_surface[y][x], water_needed);
            }
        }
        record_history_state(steps_tectonic + steps_noise + steps_hydro + step + 1, "Eco-Vegetation Growth");
    }

    std::cout << "Exporting dataset to test_realistic_map.js..." << std::endl;
    export_map_data("test_realistic_map.js", history, 512, g.scale.cell_spacing_m, g.scale.height_scale_m);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
