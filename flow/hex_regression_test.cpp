#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include "hex_orchestrator.h"
#include "hex_climate.h"
#include "hex_precipitation.h"
#include "hex_evaporation.h"
#include "hex_routing.h"
#include "hex_erosion.h"
#include "hex_landslide.h"
#include "hex_vegetation.h"
#include "hex_subsurface.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "hex_exporter.h"
#include "perlin_noise.h"

// Define constants matching the stream scenario
const int SIZE_Q = 180;
const int SIZE_R = 150;
const float HEX_RADIUS = 600.0f; // Aligned scale
const float DT = 1000.0f;        // 1000-year steps
const int TOTAL_STEPS = 1000;    // 1 million years

// Helper to convert profile names to file-safe snake_case strings
std::string get_safe_name(const std::string& name) {
    std::string safe = "";
    for (char c : name) {
        if (c == ' ') safe += "_";
        else safe += std::tolower(c);
    }
    return safe;
}

// Helper to determine write paths based on working directory (root vs flow/)
std::string get_write_path(const std::string& filename) {
    struct stat buffer;
    if (stat("flow", &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
        return "flow/" + filename;
    }
    return filename;
}

// Helper to initialize hex soil heights with a central mountain range (exactly matching hex_stream_test.cpp)
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
            
            g.H_soil[r][q] = height_m * coastal_fade;

            // Soil is thick in valleys (20m) and thin on mountain peaks (2m)
            float initial_soil_thickness = 2.0f + 18.0f * (1.0f - ridge_factor);
            g.H_bedrock[r][q] = g.H_soil[r][q] - initial_soil_thickness * coastal_fade;
            g.h_surface[r][q] = 0.0f;
            g.Q[r][q] = 0.0f;
            g.sediment[r][q] = 0.0f;
            g.vegetation[r][q] = 0.0f;
        }
    }
}

// Struct to store regression results for each individual profile
struct ProfileTestResult {
    bool failed;
    int mismatch_count;
    double mismatch_percentage;
    double ref_initial_soil, ref_final_soil, ref_net_mass, ref_veg, ref_max_q, ref_water_coverage;
    int ref_river;
    double initial_soil_volume, final_soil_volume, net_mass_change, avg_veg_cover, max_discharge, water_coverage;
    int river_cells;
};

int main(int argc, char* argv[]) {
    bool generate_mode = false;
    if (argc > 1 && std::strcmp(argv[1], "--generate") == 0) {
        generate_mode = true;
        std::cout << "--- [REGRESSION] Generation Mode Enabled ---" << std::endl;
    } else {
        std::cout << "--- [REGRESSION] Test Mode Enabled ---" << std::endl;
    }

    // Make sure output folder exists
    mkdir("flow/hex_frames", 0777);

    std::vector<ClimateProfile> profiles = ClimateRegistry::get_all_profiles();
    std::vector<ProfileTestResult> results;
    bool any_failed = false;

    for (const auto& profile : profiles) {
        std::string safe = get_safe_name(profile.name);
        std::cout << "\n==================================================" << std::endl;
        std::cout << "Evaluating Profile: " << profile.name << " (" << safe << ")" << std::endl;
        std::cout << "==================================================" << std::endl;

        // Initialize Grid & Orchestrator for each run to prevent contamination
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

        // Setup orchestrator & run simulation
        HexPhase* p1 = orchestrator.add_phase("Hydrology & Landscape Evolution", DT, TOTAL_STEPS);
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

        {
            HexRouting temp_routing;
            temp_routing.fill_depressions(g);
        }

        std::cout << "Running simulation..." << std::endl;
        orchestrator.run_phase(p1, [](int step) {});

        // Compute stats
        double final_soil_volume = 0.0;
        double total_veg_density = 0.0;
        double max_discharge = 0.0;
        int river_cells = 0;
        int land_cells = 0;
        int lake_cells = 0;
        const float SECONDS_PER_YEAR = 31557600.0f;
        auto& h_lake = g.request_field<HexLakeDepth>();

        for (int r = 0; r < SIZE_R; ++r) {
            for (int q = 0; q < SIZE_Q; ++q) {
                final_soil_volume += g.H_soil[r][q];
                float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;
                if (Q_m3s > max_discharge) max_discharge = Q_m3s;
                if (Q_m3s > 1.0f) river_cells++;
                if (g.H_soil[r][q] > 0.0f) {
                    total_veg_density += g.vegetation[r][q];
                    land_cells++;
                }
                bool is_wet = (h_lake[r][q] > profile.lake_threshold);
                if (profile.check_wetland_groundwater && g.has_field<HexGroundwater>()) {
                    auto& h_g = g.request_field<HexGroundwater>();
                    float soil_thick = g.H_soil[r][q] - g.H_bedrock[r][q];
                    if (soil_thick > 0.05f) {
                        float depth_below = soil_thick - h_g[r][q];
                        if (depth_below < 0.10f) {
                            is_wet = true;
                        }
                    }
                }
                if (is_wet) {
                    lake_cells++;
                }
            }
        }

        double net_mass_change = final_soil_volume - initial_soil_volume;
        double avg_veg_cover = land_cells > 0 ? (total_veg_density / land_cells) * 100.0 : 0.0;
        double water_coverage = (double)lake_cells / (SIZE_Q * SIZE_R) * 100.0;

        std::cout << "Budgets & Coverages:" << std::endl;
        std::cout << "  Initial Soil Volume:     " << initial_soil_volume << std::endl;
        std::cout << "  Final Soil Volume:       " << final_soil_volume << std::endl;
        std::cout << "  Net Mass Change:         " << net_mass_change << std::endl;
        std::cout << "  Average Vegetation:      " << avg_veg_cover << "%" << std::endl;
        std::cout << "  Max River Discharge:     " << max_discharge << " m^3/s" << std::endl;
        std::cout << "  Active River Cells:      " << river_cells << std::endl;
        std::cout << "  Water Coverage:          " << water_coverage << "%" << std::endl;

        // Generate Top View pixels (using scale R_px = 4.0f)
        std::vector<unsigned char> pixels;
        int w, h;
        generate_top_view_pixels(g, 4.0f, pixels, w, h, 
                                 profile.lake_threshold, profile.check_wetland_groundwater,
                                 profile.target_veg_r, profile.target_veg_g, profile.target_veg_b,
                                 profile.wet_blend_weight, profile.wet_blend_r, profile.wet_blend_g, profile.wet_blend_b);
        std::cout << "Top view generated: " << w << "x" << h << " (" << pixels.size() << " bytes)" << std::endl;

        ProfileTestResult result = {};
        result.initial_soil_volume = initial_soil_volume;
        result.final_soil_volume = final_soil_volume;
        result.net_mass_change = net_mass_change;
        result.avg_veg_cover = avg_veg_cover;
        result.max_discharge = max_discharge;
        result.river_cells = river_cells;
        result.water_coverage = water_coverage;

        if (generate_mode) {
            std::string ref_bin_save = get_write_path("hex_regression_ref_" + safe + ".bin");
            std::string ref_png_save = get_write_path("hex_regression_ref_" + safe + ".png");

            // Save binary reference
            std::ofstream out_ref(ref_bin_save, std::ios::out | std::ios::binary);
            if (!out_ref.is_open()) {
                std::cerr << "❌ Failed to save reference bin file: " << ref_bin_save << std::endl;
                return 1;
            }

            out_ref.write(reinterpret_cast<const char*>(&initial_soil_volume), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(&final_soil_volume), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(&net_mass_change), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(&avg_veg_cover), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(&max_discharge), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(&river_cells), sizeof(int));
            out_ref.write(reinterpret_cast<const char*>(&water_coverage), sizeof(double));
            out_ref.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
            out_ref.close();

            // Save PNG reference
            stbi_write_png(ref_png_save.c_str(), w, h, 3, pixels.data(), w * 3);

            std::cout << "✨ Reference baseline saved successfully to '" << ref_bin_save << "'!" << std::endl;
            std::cout << "✨ Reference PNG saved successfully to '" << ref_png_save << "'!" << std::endl;
            
            results.push_back(result);
            continue;
        }

        // Test Mode: Verify against baseline
        std::string ref_bin_path = "hex_regression_ref_" + safe + ".bin";
        {
            std::ifstream test_f(ref_bin_path);
            if (!test_f.is_open()) {
                ref_bin_path = "flow/hex_regression_ref_" + safe + ".bin";
            } else {
                test_f.close();
            }
        }

        std::ifstream in_ref(ref_bin_path, std::ios::in | std::ios::binary);
        if (!in_ref.is_open()) {
            std::cerr << "❌ Reference baseline file '" << ref_bin_path << "' not found!" << std::endl;
            std::cerr << "Run: './hex_regression_test --generate' first to record the baseline." << std::endl;
            return 1;
        }

        in_ref.read(reinterpret_cast<char*>(&result.ref_initial_soil), sizeof(double));
        in_ref.read(reinterpret_cast<char*>(&result.ref_final_soil), sizeof(double));
        in_ref.read(reinterpret_cast<char*>(&result.ref_net_mass), sizeof(double));
        in_ref.read(reinterpret_cast<char*>(&result.ref_veg), sizeof(double));
        in_ref.read(reinterpret_cast<char*>(&result.ref_max_q), sizeof(double));
        in_ref.read(reinterpret_cast<char*>(&result.ref_river), sizeof(int));
        in_ref.read(reinterpret_cast<char*>(&result.ref_water_coverage), sizeof(double));

        std::vector<unsigned char> ref_pixels(pixels.size());
        in_ref.read(reinterpret_cast<char*>(ref_pixels.data()), ref_pixels.size());
        in_ref.close();

        bool failed = false;

        // 1. Assert budgets within tolerance (allow 0.1% drift to prevent floating-point CPU compiler flakiness)
        auto assert_double = [&](const std::string& label, double val, double ref, double tol) {
            double diff = std::abs(val - ref);
            double rel_diff = ref != 0.0 ? diff / std::abs(ref) : diff;
            if (rel_diff > tol) {
                std::cerr << "  ❌ Regression failure: " << label << " differs by " << (rel_diff * 100.0) 
                          << "% (Value: " << val << ", Ref: " << ref << ", Limit: " << (tol * 100.0) << "%)" << std::endl;
                failed = true;
              } else {
                std::cout << "  ✅ " << label << " passed (Value: " << val << ", Ref: " << ref << ")" << std::endl;
            }
        };

        assert_double("Initial Soil Volume", initial_soil_volume, result.ref_initial_soil, 0.001);
        assert_double("Final Soil Volume", final_soil_volume, result.ref_final_soil, 0.001);
        assert_double("Net Mass Change", net_mass_change, result.ref_net_mass, 0.001);
        assert_double("Average Vegetation", avg_veg_cover, result.ref_veg, 0.005);
        assert_double("Max River Discharge", max_discharge, result.ref_max_q, 0.005);
        assert_double("Water Coverage", water_coverage, result.ref_water_coverage, 0.01);

        if (std::abs(river_cells - result.ref_river) > 10) {
            std::cerr << "  ❌ Regression failure: Active River Cells differs (Value: " << river_cells 
                      << ", Ref: " << result.ref_river << ", Max drift: 10)" << std::endl;
            failed = true;
        } else {
            std::cout << "  ✅ Active River Cells passed (Value: " << river_cells << ", Ref: " << result.ref_river << ")" << std::endl;
        }

        // 2. Assert top-view pixels and generate visual diff PNG
        int mismatch_count = 0;
        std::vector<unsigned char> diff_pixels(pixels.size());

        for (size_t i = 0; i < pixels.size(); i += 3) {
            if (pixels[i] != ref_pixels[i] || pixels[i+1] != ref_pixels[i+1] || pixels[i+2] != ref_pixels[i+2]) {
                mismatch_count++;
                // Neon pink highlighting for mismatching pixels
                diff_pixels[i]   = 255;
                diff_pixels[i+1] = 0;
                diff_pixels[i+2] = 128;
            } else {
                // Darkened matching background for context
                diff_pixels[i]   = (unsigned char)(pixels[i] * 0.25f);
                diff_pixels[i+1] = (unsigned char)(pixels[i+1] * 0.25f);
                diff_pixels[i+2] = (unsigned char)(pixels[i+2] * 0.25f);
            }
        }

        double mismatch_percentage = (double)mismatch_count / (w * h) * 100.0;
        if (mismatch_percentage > 0.1) {
            std::cerr << "  ❌ Regression failure: Top view image mismatch is too high (" << mismatch_percentage 
                      << "% of pixels differ, " << mismatch_count << " pixels differ, Max allowed: 0.1%)" << std::endl;
            failed = true;
        } else {
            std::cout << "  ✅ Top View Image Comparison passed (Mismatched pixels: " << mismatch_count 
                      << " / " << (w * h) << ", " << mismatch_percentage << "%)" << std::endl;
        }

        // Save files with profile safe name
        std::string current_png_save = get_write_path("hex_frames/frame_1000_" + safe + ".png");
        std::string diff_png_save = get_write_path("hex_regression_diff_" + safe + ".png");

        stbi_write_png(current_png_save.c_str(), w, h, 3, pixels.data(), w * 3);
        stbi_write_png(diff_png_save.c_str(), w, h, 3, diff_pixels.data(), w * 3);

        result.failed = failed;
        result.mismatch_count = mismatch_count;
        result.mismatch_percentage = mismatch_percentage;

        if (failed) any_failed = true;
        results.push_back(result);
    }

    if (generate_mode) {
        return 0;
    }

    // Save global JSON stats report compiling all profiles
    std::string stats_json_save = get_write_path("hex_regression_stats.json");
    std::ofstream out_json(stats_json_save);
    if (out_json.is_open()) {
        out_json << "{\n"
                 << "  \"status\": \"" << (any_failed ? "FAILED" : "PASSED") << "\",\n"
                 << "  \"profiles\": {\n";
        
        for (size_t p = 0; p < profiles.size(); ++p) {
            const auto& prof = profiles[p];
            std::string safe = get_safe_name(prof.name);
            const auto& res = results[p];

            out_json << "    \"" << prof.name << "\": {\n"
                     << "      \"status\": \"" << (res.failed ? "FAILED" : "PASSED") << "\",\n"
                     << "      \"mismatch_count\": " << res.mismatch_count << ",\n"
                     << "      \"mismatch_percentage\": " << res.mismatch_percentage << ",\n"
                     << "      \"baseline\": {\n"
                     << "        \"initial_soil_volume\": " << res.ref_initial_soil << ",\n"
                     << "        \"final_soil_volume\": " << res.ref_final_soil << ",\n"
                     << "        \"net_mass_change\": " << res.ref_net_mass << ",\n"
                     << "        \"avg_vegetation_cover\": " << res.ref_veg << ",\n"
                     << "        \"max_river_discharge\": " << res.ref_max_q << ",\n"
                     << "        \"active_river_cells\": " << res.ref_river << ",\n"
                     << "        \"water_coverage\": " << res.ref_water_coverage << "\n"
                     << "      },\n"
                     << "      \"current\": {\n"
                     << "        \"initial_soil_volume\": " << res.initial_soil_volume << ",\n"
                     << "        \"final_soil_volume\": " << res.final_soil_volume << ",\n"
                     << "        \"net_mass_change\": " << res.net_mass_change << ",\n"
                     << "        \"avg_vegetation_cover\": " << res.avg_veg_cover << ",\n"
                     << "        \"max_river_discharge\": " << res.max_discharge << ",\n"
                     << "        \"active_river_cells\": " << res.river_cells << ",\n"
                     << "        \"water_coverage\": " << res.water_coverage << "\n"
                     << "      }\n"
                     << "    }" << (p + 1 < profiles.size() ? "," : "") << "\n";
        }
        out_json << "  }\n"
                 << "}\n";
        out_json.close();
    }

    if (any_failed) {
        std::cout << "\n❌ REGRESSION TEST SUITE FAILED!" << std::endl;
        return 1;
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << "✨ ALL REGRESSION TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
