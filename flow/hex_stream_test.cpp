#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include "hex_orchestrator.h"
#include "hex_precipitation.h"
#include "hex_routing.h"
#include "hex_erosion.h"
#include "hex_landslide.h"
#include "hex_vegetation.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "hex_exporter.h"
#include "perlin_noise.h"

// Helper to initialize hex soil heights with a central mountain range
void initialize_hex_soil_perlin(HexGrid& g) {
    PerlinNoise2D perlin;
    for (int r = 0; r < g.size_r; ++r) {
        for (int q = 0; q < g.size_q; ++q) {
            // Baseline fractal noise
            float pn = perlin.noise(q * 0.08f, r * 0.08f) + 0.35f * perlin.noise(q * 0.22f, r * 0.22f);
            
            // Build a diagonal mountain range ridge
            float center_q = g.size_q * 0.5f;
            float center_r = g.size_r * 0.5f;
            
            // Distance to a diagonal line running from top-left to bottom-right
            float dist_to_ridge = std::abs((q - center_q) + (r - center_r)) / 1.4142f;
            // Locks mountain span to original 90x75 configuration (~31.5 units wide)
            float ridge_factor = std::max(0.0f, 1.0f - dist_to_ridge / 31.5f);

            // Coastal gradient (lowest heights near grid boundaries)
            float edge_dist_q = std::min(q, g.size_q - 1 - q);
            float edge_dist_r = std::min(r, g.size_r - 1 - r);
            float min_edge_dist = std::min(edge_dist_q, edge_dist_r);
            float coastal_fade = std::min(1.0f, min_edge_dist / 6.0f);

            // Base elevation in meters
            float height_m = (0.15f + pn * 0.4f + ridge_factor * 1.8f) * 650.0f;
            
            // Fade elevations to coastal sea level (0.0) near boundaries
            g.H_soil[r][q] = height_m * coastal_fade;
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "Starting Hexagonal Grid Stream Formation Simulation" << std::endl;
    std::cout << "==================================================" << std::endl;

    const int SIZE_Q = 180;
    const int SIZE_R = 150;
    const float HEX_RADIUS = 5000.0f; // 5 km
    const float DT = 1.0f;            // 1.0 year steps
    const int TOTAL_STEPS = 100;
    const int EXPORT_STRIDE = 5;

    // Create directories for exporting frames
    mkdir("flow/hex_frames", 0777);

    // 1. Initialize HexGrid
    HexOrchestrator orchestrator(SIZE_Q, SIZE_R);
    HexGrid& g = orchestrator.get_grid();
    g.scale.hex_radius_m = HEX_RADIUS;
    initialize_hex_soil_perlin(g);

    // Save initial state for history comparison
    double initial_soil_volume = 0.0;
    for (int r = 0; r < SIZE_R; ++r) {
        for (int q = 0; q < SIZE_Q; ++q) {
            initial_soil_volume += g.H_soil[r][q];
        }
    }

    // 2. Setup Multi-Phase Simulation
    HexPhase* p1 = orchestrator.add_phase("Hydrology & Landscape Evolution", DT, TOTAL_STEPS);
    p1->add<HexPrecipitation>(1.4f);                 // 1.4 m rainfall per year
    p1->add<HexRouting>(0.15f, 0.40f, true);         // D6 routing with sink filling
    p1->add<HexErosion>(2.5e-5f, 0.04f, 0.15f, 2.0f); // Stream power + sediment transport
    p1->add<HexLandslide>(0.14f, 0.50f);             // Stable slopes (14%) + veg cohesion
    p1->add<HexVegetation>(0.16f, 1.0f, 0.8f);       // Vegetation growth

    std::vector<HexGridState> history;

    // Save step 0
    HexGridState init_state;
    init_state.step = 0;
    init_state.phase_name = "Genesis";
    init_state.H_soil = g.H_soil;
    init_state.h_surface = g.h_surface;
    init_state.Q = g.Q;
    init_state.sediment = g.sediment;
    init_state.vegetation = g.vegetation;
    init_state.downstream_dir = g.downstream_dir;
    history.push_back(init_state);
    save_hex_png("flow/hex_frames/frame_0.png", g, 4.0f); // 2D top-down view
    save_hex_png_3d("flow/hex_frames/frame_3d_0.png", g, 4.0f); // 3D isometric view

    // 3. Run Simulation
    std::cout << "Running 100 Annual Time-Steps (dt = 1.0 year)..." << std::endl;
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
            state.Q = g.Q;
            state.sediment = g.sediment;
            state.vegetation = g.vegetation;
            state.downstream_dir = g.downstream_dir;
            history.push_back(state);

            std::string frame_filename = "flow/hex_frames/frame_" + std::to_string(current_step) + ".png";
            save_hex_png(frame_filename, g, 4.0f);

            std::string frame_filename_3d = "flow/hex_frames/frame_3d_" + std::to_string(current_step) + ".png";
            save_hex_png_3d(frame_filename_3d, g, 4.0f);
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
            if (g.Q[r][q] / 31557600.0f > 1.0f) { // River discharge > 1.0 m^3/s
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
    std::cout << "Saving binary simulation history to 'flow/test_hex_stream.bin'..." << std::endl;
    export_hex_to_binary("flow/test_hex_stream.bin", history, SIZE_Q, SIZE_R, HEX_RADIUS);
    std::cout << "SUCCESS: Binary export completed!" << std::endl;
    std::cout << "Check 'flow/hex_frames/' for shaded 2D hexagon visualizations." << std::endl;

    return 0;
}
