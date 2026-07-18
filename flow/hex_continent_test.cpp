#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <random>
#include "hex_orchestrator.h"
#include "hex_climate.h"
#include "hex_precipitation.h"
#include "hex_evaporation.h"
#include "hex_routing.h"
#include "hex_erosion.h"
#include "hex_landslide.h"
#include "hex_vegetation.h"
#include "hex_subsurface.h"
#include "hex_wave_erosion.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "hex_exporter.h"
#include "perlin_noise.h"

// Helper to determine write paths based on working directory (root vs flow/)
std::string get_write_path(const std::string& filename) {
    struct stat buffer;
    if (stat("flow", &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return "flow/" + filename;
    }
    return filename;
}

void apply_multipoint_island(HexGrid& g, float center_height, float ocean_depth, float noise_scale, float noise_amp, int num_seeds, float spread_radius) {
    float center_q = g.size_q * 0.5f;
    float center_r = g.size_r * 0.5f;
    
    float cx = g.scale.hex_radius_m * 1.7320508f * (center_q + center_r * 0.5f);
    float cy = g.scale.hex_radius_m * 1.5f * center_r;

    // Use a deterministic standard C++ PRNG
    std::mt19937 gen(1337);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    struct Point { float x, y; };
    std::vector<Point> seeds;
    for (int i = 0; i < num_seeds; ++i) {
        float angle = dis(gen) * 2.0f * 3.14159265f;
        float r_dist = dis(gen) * spread_radius;
        seeds.push_back({ cx + r_dist * std::cos(angle), cy + r_dist * std::sin(angle) });
    }

    PerlinNoise2D perlin;

    // Dynamically calculate the minimum effective distance on any border cell to prevent land contact
    float min_border_eff_dist = 1e9f;
    for (int r = 0; r < g.size_r; ++r) {
        for (int q = 0; q < g.size_q; ++q) {
            bool is_border = (r == 0 || r == g.size_r - 1 || q == 0 || q == g.size_q - 1);
            if (is_border) {
                float x = g.scale.hex_radius_m * 1.7320508f * (q + r * 0.5f);
                float y = g.scale.hex_radius_m * 1.5f * r;
                
                float min_d = 1e9f;
                for (const auto& seed : seeds) {
                    float dx = x - seed.x;
                    float dy = y - seed.y;
                    float d = std::sqrt(dx*dx + dy*dy);
                    min_d = std::min(min_d, d);
                }
                
                float pn = perlin.noise(q * noise_scale, r * noise_scale) + 0.35f * perlin.noise(q * 2.75f * noise_scale, r * 2.75f * noise_scale);
                float eff_dist = std::max(0.0f, min_d + pn * noise_amp);
                min_border_eff_dist = std::min(min_border_eff_dist, eff_dist);
            }
        }
    }

    // Set maximum landmass radius so that the land shoreline (elevation 0.0m, which occurs at t = 0.391826f)
    // stays at least 12km away from the borders (doubled buffer for doubled map size).
    float t_shoreline = 0.391826f;
    float active_max_dist_m = std::max(0.0f, (min_border_eff_dist - 12000.0f) / t_shoreline);

    for (int r = 0; r < g.size_r; ++r) {
        for (int q = 0; q < g.size_q; ++q) {
            float x = g.scale.hex_radius_m * 1.7320508f * (q + r * 0.5f);
            float y = g.scale.hex_radius_m * 1.5f * r;

            float min_d = 1e9f;
            for (const auto& seed : seeds) {
                float dx = x - seed.x;
                float dy = y - seed.y;
                float d = std::sqrt(dx*dx + dy*dy);
                min_d = std::min(min_d, d);
            }

            float pn = perlin.noise(q * noise_scale, r * noise_scale) + 0.35f * perlin.noise(q * 2.75f * noise_scale, r * 2.75f * noise_scale);
            float eff_dist = std::max(0.0f, min_d + pn * noise_amp);

            float h = ocean_depth;
            if (eff_dist < active_max_dist_m) {
                float t = eff_dist / active_max_dist_m;
                float factor = 0.5f * (1.0f + std::cos(t * 3.14159265f));
                h = ocean_depth + (center_height - ocean_depth) * factor;
            }
            g.H_soil[r][q] = h;
        }
    }
}

// Helper to initialize hex soil heights with a central mountain range
void initialize_hex_soil_perlin(HexGrid& g) {
    // 150m peak height, -300m ocean floor.
    // noise_scale = 0.025f, noise_amp = 24000.0f, num_seeds = 8, spread_radius = 24000.0f
    apply_multipoint_island(g, 150.0f, -300.0f, 0.025f, 24000.0f, 8, 24000.0f);

    // Overlay high-frequency detail noise
    PerlinNoise2D perlin;
    for (int r = 0; r < g.size_r; ++r) {
        for (int q = 0; q < g.size_q; ++q) {
            float detail = perlin.noise(q * 0.16f, r * 0.16f) * 12.0f;
            g.H_soil[r][q] += detail;
            g.H_bedrock[r][q] = g.H_soil[r][q] - 15.0f; // 15m soil thickness
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Starting Doubled Hexagonal Grid Continental Simulation" << std::endl;
    std::cout << "==================================================" << std::endl;

    const int SIZE_Q = 360;
    const int SIZE_R = 300;
    const float HEX_RADIUS = 600.0f;  // 600 m
    const float DT = 1000.0f;         // 1000 year steps
    const int TOTAL_STEPS = 1000;
    const int EXPORT_STRIDE = 5;

    // Create directories for exporting frames
    mkdir("flow/hex_frames", 0777);

    // 1. Initialize HexGrid
    HexOrchestrator orchestrator(SIZE_Q, SIZE_R);
    HexGrid& g = orchestrator.get_grid();
    g.scale.hex_radius_m = HEX_RADIUS;
    initialize_hex_soil_perlin(g);

    double initial_soil_volume = 0.0;
    for (int r = 0; r < SIZE_R; ++r) {
        for (int q = 0; q < SIZE_Q; ++q) {
            initial_soil_volume += g.H_soil[r][q];
        }
    }

    // 2. Setup Multi-Phase Simulation
    HexPhase* p1 = orchestrator.add_phase("Hydrology & Landscape Evolution", DT, TOTAL_STEPS);
    
    // Load Littoral Coastline profile from registry
    ClimateProfile profile = ClimateRegistry::get_profile("Littoral Coastline");

    p1->add<HexPrecipitation>(profile);
    p1->add<HexEvaporation>(profile, profile.evap_coefficient);
    p1->add<HexSubsurface>(profile);
    p1->add<HexRouting>(profile, HexRoutingParams{
        .depth_coefficient = 0.15f,
        .depth_exponent = 0.40f,
        .run_sink_fill = true,
        .evap_coefficient = profile.evap_coefficient
    });
    p1->add<HexErosion>(1.0e-2f, 0.04f, 0.15f, 2.0f);
    p1->add<HexLandslide>(0.14f, 0.50f);
    p1->add<HexVegetation>(profile, 1.0f);

    std::vector<HexGridState> history;

    // Run depression filling once to calculate initial lake depths at Year 0
    {
        HexRouting temp_routing;
        temp_routing.fill_depressions(g);
    }

    // Save step 0
    HexGridState init_state;
    init_state.step = 0;
    init_state.phase_name = "Genesis";
    init_state.H_soil = g.H_soil;
    init_state.h_surface = g.h_surface;
    init_state.h_lake = g.request_field<HexLakeDepth>();
    init_state.Q = g.Q;
    init_state.sediment = g.sediment;
    init_state.vegetation = g.vegetation;
    init_state.downstream_dir = g.downstream_dir;
    history.push_back(init_state);

    // 3. Run Simulation
    std::cout << "Running " << TOTAL_STEPS << " Annual Time-Steps (dt = " << DT << " year)..." << std::endl;
    orchestrator.run_phase(p1, [&](int step) {
        int current_step = step + 1;

        // Check for NaNs
        for (int r = 0; r < SIZE_R; ++r) {
            for (int q = 0; q < SIZE_Q; ++q) {
                if (std::isnan(g.H_soil[r][q]) || std::isnan(g.h_surface[r][q]) || std::isnan(g.Q[r][q])) {
                    std::cerr << "CRITICAL: NaN detected at step " << current_step 
                              << " at cell (" << q << "," << r << ")" << std::endl;
                    exit(1);
                }
            }
        }

        // Export state history
        if (current_step % EXPORT_STRIDE == 0 || current_step == TOTAL_STEPS) {
            HexGridState state;
            state.step = current_step;
            state.phase_name = p1->name;
            state.H_soil = g.H_soil;
            state.h_surface = g.h_surface;
            state.h_lake = g.request_field<HexLakeDepth>();
            state.Q = g.Q;
            state.sediment = g.sediment;
            state.vegetation = g.vegetation;
            state.downstream_dir = g.downstream_dir;
            history.push_back(state);
        }
    });

    // 4. Print Telemetry & Statistics
    double final_soil_volume = 0.0;
    double total_veg_density = 0.0;
    double max_discharge = 0.0;
    int river_cells = 0;

    for (int r = 0; r < SIZE_R; ++r) {
        for (int q = 0; q < SIZE_Q; ++q) {
            final_soil_volume += g.H_soil[r][q];
            total_veg_density += g.vegetation[r][q];
            if (g.Q[r][q] > max_discharge) {
                max_discharge = g.Q[r][q];
            }
            if (g.Q[r][q] / 31557600.0f > 1.0f) {
                river_cells++;
            }
        }
    }

    std::cout << "\n--- Simulation Completed Successfully! ---" << std::endl;
    std::cout << "Initial Soil Mass Index: " << initial_soil_volume << " m" << std::endl;
    std::cout << "Final Soil Mass Index:   " << final_soil_volume << " m" << std::endl;
    std::cout << "Net Mass Change:         " << (final_soil_volume - initial_soil_volume) << " m" << std::endl;
    std::cout << "Average Vegetation Cover: " << (total_veg_density / (SIZE_Q * SIZE_R)) * 100.0f << "%" << std::endl;
    std::cout << "Max River Discharge:     " << max_discharge / 31557600.0f << " m^3/s" << std::endl;
    std::cout << "Active River Cells:      " << river_cells << " / " << (SIZE_Q * SIZE_R) << std::endl;

    // 5. Binary Export
    std::string export_file = get_write_path("test_hex_continent_large.bin");
    std::cout << "Saving binary simulation history to '" << export_file << "'..." << std::endl;
    export_hex_to_binary(export_file, history, SIZE_Q, SIZE_R, HEX_RADIUS, DT);
    std::cout << "SUCCESS: Binary export completed!" << std::endl;

    return 0;
}
