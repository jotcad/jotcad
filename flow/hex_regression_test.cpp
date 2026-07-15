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
#include "hex_routing.h"
#include "hex_erosion.h"
#include "hex_landslide.h"
#include "hex_vegetation.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "hex_exporter.h"
#include "perlin_noise.h"

// Define constants matching the stream scenario
const int SIZE_Q = 180;
const int SIZE_R = 150;
const float HEX_RADIUS = 600.0f; // Aligned scale
const float DT = 1000.0f;        // 1000-year steps
const int TOTAL_STEPS = 1000;    // 1 million years

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

// Generate the 2D Top View pixel buffer matching save_hex_png
void generate_top_view_pixels(const HexGrid& g, float R_px, std::vector<unsigned char>& pixels, int& w, int& h) {
    auto& h_lake = const_cast<HexGrid&>(g).request_field<HexLakeDepth>();
    float max_x = R_px * 1.7320508f * ((g.size_q - 1) + (g.size_r - 1) * 0.5f);
    float max_y = R_px * 1.5f * (g.size_r - 1);

    float margin_x = R_px * 1.7320508f * 1.5f;
    float margin_y = R_px * 1.5f * 1.5f;

    w = std::ceil(max_x + 2.0f * margin_x);
    h = std::ceil(max_y + 2.0f * margin_y);

    pixels.assign(w * h * 3, 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float X_adj = x - margin_x;
            float Y_adj = (h - 1 - y) - margin_y;

            float q_f = (0.577350269f * X_adj - 0.333333333f * Y_adj) / R_px;
            float r_f = (0.666666667f * Y_adj) / R_px;

            float x_f = q_f;
            float z_f = r_f;
            float y_f = -x_f - z_f;

            int rx = std::round(x_f);
            int ry = std::round(y_f);
            int rz = std::round(z_f);

            float x_diff = std::abs(rx - x_f);
            float y_diff = std::abs(ry - y_f);
            float z_diff = std::abs(rz - z_f);

            if (x_diff > y_diff && x_diff > z_diff) {
                rx = -ry - rz;
            } else if (y_diff > z_diff) {
                ry = -rx - rz;
            } else {
                rz = -rx - ry;
            }

            int q = rx;
            int r = rz;

            unsigned char pr = 15, pg = 23, pb = 42; // Deep space background

            if (g.is_valid(q, r)) {
                float H = g.H_soil[r][q];
                float water = g.h_surface[r][q] + h_lake[r][q];
                float veg = g.vegetation[r][q];

                float sr = 0.0f, sg = 0.0f, sb = 0.0f;
                float arability = calculate_cell_arability(g, h_lake, q, r);
                float alpha_arab = arability * 0.35f;

                if (H > 700.0f) {
                    float snow_t = std::min(1.0f, (H - 700.0f) / 300.0f);
                    float rock_r = 110.0f;
                    float rock_g = 105.0f;
                    float rock_b = 100.0f;
                    float snow_val = 245.0f;

                    sr = (1.0f - snow_t) * rock_r + snow_t * snow_val;
                    sg = (1.0f - snow_t) * rock_g + snow_t * snow_val;
                    sb = (1.0f - snow_t) * rock_b + snow_t * snow_val;

                    sr = (1.0f - alpha_arab) * sr + alpha_arab * 235.0f;
                    sg = (1.0f - alpha_arab) * sg + alpha_arab * 45.0f;
                    sb = (1.0f - alpha_arab) * sb + alpha_arab * 45.0f;
                } else {
                    float Q_m3s = g.Q[r][q] / 31557600.0f;
                    float moisture_factor = std::min(1.0f, (Q_m3s / 30.0f) + (water / 0.5f));

                    float target_r, target_g, target_b;
                    if (moisture_factor >= 0.35f) {
                        target_r = 20.0f; target_g = 120.0f; target_b = 45.0f;
                    } else {
                        target_r = 185.0f; target_g = 230.0f; target_b = 85.0f;
                    }

                    float soil_t_m = H - g.H_bedrock[r][q];
                    float bedrock_blend = std::max(0.0f, std::min(1.0f, (0.50f - soil_t_m) / 0.50f));
                    
                    float substrate_r = (1.0f - bedrock_blend) * 195.0f + bedrock_blend * 135.0f;
                    float substrate_g = (1.0f - bedrock_blend) * 178.0f + bedrock_blend * 135.0f;
                    float substrate_b = (1.0f - bedrock_blend) * 130.0f + bedrock_blend * 135.0f;

                    float sub_r = (1.0f - alpha_arab) * substrate_r + alpha_arab * 235.0f;
                    float sub_g = (1.0f - alpha_arab) * substrate_g + alpha_arab * 45.0f;
                    float sub_b = (1.0f - alpha_arab) * substrate_b + alpha_arab * 45.0f;

                    sr = (1.0f - veg) * sub_r + veg * target_r;
                    sg = (1.0f - veg) * sub_g + veg * target_g;
                    sb = (1.0f - veg) * sub_b + veg * target_b;
                }

                pr = (unsigned char)std::max(0.0f, std::min(255.0f, sr));
                pg = (unsigned char)std::max(0.0f, std::min(255.0f, sg));
                pb = (unsigned char)std::max(0.0f, std::min(255.0f, sb));

                if (water > 0.50f) {
                    float w_norm = std::min(1.0f, (water - 0.50f) / 1.00f);
                    float wr = 38.0f + (3.0f - 38.0f) * w_norm;
                    float wg = 145.0f + (80.0f - 145.0f) * w_norm;
                    float wb = 224.0f + (150.0f - 224.0f) * w_norm;

                    float opacity = w_norm * 0.92f;
                    pr = (unsigned char)((1.0f - opacity) * pr + opacity * wr);
                    pg = (unsigned char)((1.0f - opacity) * pg + opacity * wg);
                    pb = (unsigned char)((1.0f - opacity) * pb + opacity * wb);
                }

                // Boundary outline & topological contour lines
                float cx = R_px * 1.7320508f * (q + r * 0.5f);
                float cy = R_px * 1.5f * r;

                float dx_pixel = X_adj - cx;
                float dy_pixel = Y_adj - cy;

                float d0 = std::abs(dx_pixel * 0.8660254f + dy_pixel * 0.5f);
                float d1 = std::abs(dy_pixel);
                float d2 = std::abs(-dx_pixel * 0.8660254f + dy_pixel * 0.5f);

                float edge_dist = std::max({d0, d1, d2});

                if (edge_dist >= (0.8660254f * R_px - 0.70f)) {
                    pr = (unsigned char)(pr * 0.45f);
                    pg = (unsigned char)(pg * 0.45f);
                    pb = (unsigned char)(pb * 0.45f);

                    int closest_qn = q;
                    int closest_rn = r;
                    float min_dist_sq = 1e9f;

                    const int HEX_DQ[6] = {1, 1, 0, -1, -1, 0};
                    const int HEX_DR[6] = {0, -1, -1, 0, 1, 1};

                    for (int i = 0; i < 6; ++i) {
                        int nq = q + HEX_DQ[i];
                        int nr = r + HEX_DR[i];
                        if (g.is_valid(nq, nr)) {
                            float ncx = R_px * 1.7320508f * (nq + nr * 0.5f);
                            float ncy = R_px * 1.5f * nr;
                            float ndx = X_adj - ncx;
                            float ndy = Y_adj - ncy;
                            float dist_sq = ndx * ndx + ndy * ndy;
                            if (dist_sq < min_dist_sq) {
                                min_dist_sq = dist_sq;
                                closest_qn = nq;
                                closest_rn = nr;
                            }
                        }
                    }

                    if (closest_qn != q || closest_rn != r) {
                        float H_C = H;
                        float H_N = g.H_soil[closest_rn][closest_qn];

                        const float CONTOUR_INTERVAL = 150.0f;
                        int band_C = std::floor(H_C / CONTOUR_INTERVAL);
                        int band_N = std::floor(H_N / CONTOUR_INTERVAL);

                        if (band_C != band_N) {
                            pr = 220; pg = 150; pb = 60;
                        }
                    }
                }
            }

            int idx_pixel = (y * w + x) * 3;
            pixels[idx_pixel] = pr;
            pixels[idx_pixel + 1] = pg;
            pixels[idx_pixel + 2] = pb;
        }
    }
}

int main(int argc, char* argv[]) {
    bool generate_mode = false;
    if (argc > 1 && std::strcmp(argv[1], "--generate") == 0) {
        generate_mode = true;
        std::cout << "--- [REGRESSION] Generation Mode Enabled ---" << std::endl;
    } else {
        std::cout << "--- [REGRESSION] Test Mode Enabled ---" << std::endl;
    }

    // Initialize Grid & Orchestrator
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
    
    ClimateProfile profile = ClimateRegistry::get_profile("Subtropical Highland");
    p1->add<HexPrecipitation>(profile);
    p1->add<HexRouting>(0.15f, 0.40f, true);
    p1->add<HexErosion>(1.0e-2f, 0.04f, 0.15f, 2.0f);
    p1->add<HexLandslide>(0.14f, 0.50f);
    p1->add<HexVegetation>(profile, 1.0f);

    {
        HexRouting temp_routing;
        temp_routing.fill_depressions(g);
    }

    // Make sure output folder exists
    mkdir("flow/hex_frames", 0777);

    std::cout << "Running simulation..." << std::endl;
    orchestrator.run_phase(p1, [](int step) {});

    // Compute stats
    double final_soil_volume = 0.0;
    double total_veg_density = 0.0;
    double max_discharge = 0.0;
    int river_cells = 0;
    int land_cells = 0;
    const float SECONDS_PER_YEAR = 31557600.0f;

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
        }
    }

    double net_mass_change = final_soil_volume - initial_soil_volume;
    double avg_veg_cover = land_cells > 0 ? (total_veg_density / land_cells) * 100.0 : 0.0;

    std::cout << "Budgets & Coverages:" << std::endl;
    std::cout << "  Initial Soil Volume:     " << initial_soil_volume << std::endl;
    std::cout << "  Final Soil Volume:       " << final_soil_volume << std::endl;
    std::cout << "  Net Mass Change:         " << net_mass_change << std::endl;
    std::cout << "  Average Vegetation:      " << avg_veg_cover << "%" << std::endl;
    std::cout << "  Max River Discharge:     " << max_discharge << " m^3/s" << std::endl;
    std::cout << "  Active River Cells:      " << river_cells << std::endl;

    // Generate Top View pixels (using scale R_px = 4.0f)
    std::vector<unsigned char> pixels;
    int w, h;
    generate_top_view_pixels(g, 4.0f, pixels, w, h);
    std::cout << "Top view generated: " << w << "x" << h << " (" << pixels.size() << " bytes)" << std::endl;

    // Dynamic path resolution for reference file
    std::string ref_bin_path = "hex_regression_ref.bin";
    {
        std::ifstream test_f(ref_bin_path);
        if (!test_f.is_open()) {
            ref_bin_path = "flow/hex_regression_ref.bin";
        } else {
            test_f.close();
        }
    }

    if (generate_mode) {
        // Resolve save paths
        std::string ref_bin_save = get_write_path("hex_regression_ref.bin");
        std::string ref_png_save = get_write_path("hex_regression_ref.png");

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
        out_ref.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
        out_ref.close();

        // Save PNG reference
        stbi_write_png(ref_png_save.c_str(), w, h, 3, pixels.data(), w * 3);

        std::cout << "✨ Reference baseline saved successfully to '" << ref_bin_save << "'!" << std::endl;
        std::cout << "✨ Reference PNG saved successfully to '" << ref_png_save << "'!" << std::endl;
        return 0;
    }

    // Test Mode: Verify against baseline
    std::ifstream in_ref(ref_bin_path, std::ios::in | std::ios::binary);
    if (!in_ref.is_open()) {
        std::cerr << "❌ Reference baseline file '" << ref_bin_path << "' not found!" << std::endl;
        std::cerr << "Run: './hex_regression_test --generate' first to record the baseline." << std::endl;
        return 1;
    }

    double ref_initial_soil, ref_final_soil, ref_net_mass, ref_veg, ref_max_q;
    int ref_river;

    in_ref.read(reinterpret_cast<char*>(&ref_initial_soil), sizeof(double));
    in_ref.read(reinterpret_cast<char*>(&ref_final_soil), sizeof(double));
    in_ref.read(reinterpret_cast<char*>(&ref_net_mass), sizeof(double));
    in_ref.read(reinterpret_cast<char*>(&ref_veg), sizeof(double));
    in_ref.read(reinterpret_cast<char*>(&ref_max_q), sizeof(double));
    in_ref.read(reinterpret_cast<char*>(&ref_river), sizeof(int));

    std::vector<unsigned char> ref_pixels(pixels.size());
    in_ref.read(reinterpret_cast<char*>(ref_pixels.data()), ref_pixels.size());
    in_ref.close();

    bool failed = false;

    // 1. Assert budgets within tolerance
    auto assert_double = [&](const std::string& label, double val, double ref, double tol) {
        double diff = std::abs(val - ref);
        double rel_diff = ref != 0.0 ? diff / std::abs(ref) : diff;
        if (rel_diff > tol) {
            std::cerr << "❌ Regression failure: " << label << " differs by " << (rel_diff * 100.0) 
                      << "% (Value: " << val << ", Ref: " << ref << ", Limit: " << (tol * 100.0) << "%)" << std::endl;
            failed = true;
        } else {
            std::cout << "✅ " << label << " passed (Value: " << val << ", Ref: " << ref << ")" << std::endl;
        }
    };

    assert_double("Initial Soil Volume", initial_soil_volume, ref_initial_soil, 0.001);
    assert_double("Final Soil Volume", final_soil_volume, ref_final_soil, 0.001);
    assert_double("Net Mass Change", net_mass_change, ref_net_mass, 0.001);
    assert_double("Average Vegetation", avg_veg_cover, ref_veg, 0.005);
    assert_double("Max River Discharge", max_discharge, ref_max_q, 0.005);

    if (std::abs(river_cells - ref_river) > 10) {
        std::cerr << "❌ Regression failure: Active River Cells differs (Value: " << river_cells 
                  << ", Ref: " << ref_river << ", Max drift: 10)" << std::endl;
        failed = true;
    } else {
        std::cout << "✅ Active River Cells passed (Value: " << river_cells << ", Ref: " << ref_river << ")" << std::endl;
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
        std::cerr << "❌ Regression failure: Top view image mismatch is too high (" << mismatch_percentage 
                  << "% of pixels differ, " << mismatch_count << " pixels differ, Max allowed: 0.1%)" << std::endl;
        failed = true;
    } else {
        std::cout << "✅ Top View Image Comparison passed (Mismatched pixels: " << mismatch_count 
                  << " / " << (w * h) << ", " << mismatch_percentage << "%)" << std::endl;
    }

    // Save current top view and diff view to files
    std::string current_png_save = get_write_path("hex_frames/frame_1000.png");
    std::string diff_png_save = get_write_path("hex_regression_diff.png");
    std::string stats_json_save = get_write_path("hex_regression_stats.json");

    stbi_write_png(current_png_save.c_str(), w, h, 3, pixels.data(), w * 3);
    stbi_write_png(diff_png_save.c_str(), w, h, 3, diff_pixels.data(), w * 3);

    // Save JSON statistics report
    std::ofstream out_json(stats_json_save);
    if (out_json.is_open()) {
        out_json << "{\n"
                 << "  \"status\": \"" << (failed ? "FAILED" : "PASSED") << "\",\n"
                 << "  \"mismatch_count\": " << mismatch_count << ",\n"
                 << "  \"mismatch_percentage\": " << mismatch_percentage << ",\n"
                 << "  \"baseline\": {\n"
                 << "    \"initial_soil_volume\": " << ref_initial_soil << ",\n"
                 << "    \"final_soil_volume\": " << ref_final_soil << ",\n"
                 << "    \"net_mass_change\": " << ref_net_mass << ",\n"
                 << "    \"avg_vegetation_cover\": " << ref_veg << ",\n"
                 << "    \"max_river_discharge\": " << ref_max_q << ",\n"
                 << "    \"active_river_cells\": " << ref_river << "\n"
                 << "  },\n"
                 << "  \"current\": {\n"
                 << "    \"initial_soil_volume\": " << initial_soil_volume << ",\n"
                 << "    \"final_soil_volume\": " << final_soil_volume << ",\n"
                 << "    \"net_mass_change\": " << net_mass_change << ",\n"
                 << "    \"avg_vegetation_cover\": " << avg_veg_cover << ",\n"
                 << "    \"max_river_discharge\": " << max_discharge << ",\n"
                 << "    \"active_river_cells\": " << river_cells << "\n"
                 << "  }\n"
                 << "}\n";
        out_json.close();
    }

    if (failed) {
        std::cout << "❌ REGRESSION TEST FAILED!" << std::endl;
        return 1;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "✨ REGRESSION TEST PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
