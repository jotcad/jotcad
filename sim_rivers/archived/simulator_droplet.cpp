#include "simulator.h"
#include "noise.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace sim_rivers {

Simulator::Simulator(int w, int h)
    : width(w), height(h), scenario("standard"), step_count(0), seed(1337) {
    grid.resize(width * height);
}

Simulator::~Simulator() {}

void Simulator::initialize(unsigned int s) {
    seed = s;
    rng.seed(seed);
    PerlinNoise noiseGen(seed);
    step_count = 0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            Cell& cell = grid[idx];
            cell.water = 0.0f;
            cell.groundwater = 0.0f;
            cell.sediment = 0.0f;
            cell.flow_acc = 1.0f;
            cell.flow_target_idx = -1;

            if (scenario == "valley_test") {
                // Gentle valley floor with tiny lateral slope and erodible soil noise
                float center_x = width / 2.0f;
                float dist_x = std::abs(x - center_x);
                // Very gentle V-shape (0.1m drop over 32m)
                float v_slope = (dist_x / (width / 2.0f)) * 0.1f;
                // Winding downhill slope (1.0m drop over 64m)
                float y_slope = (1.0f - (float)y / (height - 1)) * 1.0f;
                
                // Add 8mm noise to soil layer to initiate winding path (easily erodible)
                float nx = (float)x / (width - 1);
                float ny = (float)y / (height - 1);
                float noise = noiseGen.fBm(nx * 4.0f, ny * 4.0f, 3, 2.0, 0.5f) * 0.008f;
                
                cell.bedrock = 15.0f + v_slope + y_slope;
                cell.soil = 0.5f + noise; // Uniform 50cm soil + erodible noise
            } else {
                // Standard noise-based terrain
                float nx = (float)x / (width - 1);
                float ny = (float)y / (height - 1);
                float base_h = 10.0f + noiseGen.fBm(nx * 1.5f, ny * 1.5f, 4, 2.0, 0.45) * 8.0f;
                float ridged_h = noiseGen.ridged(nx * 2.0f, ny * 2.0f, 5, 2.0, 0.5) * 12.0f;
                cell.bedrock = base_h + ridged_h;
                cell.soil = 1.0f;
            }
        }
    }
}

void Simulator::step(float dt) {
    step_count++;
    (void)dt;

    // 1. Spawners & Droplet Simulation Loop (2,000 droplets per step)
    int num_droplets = 1500;
    std::vector<float> flow_vol(width * height, 0.0f);

    std::uniform_real_distribution<float> dist_jitter(-0.4f, 0.4f);
    std::uniform_real_distribution<float> dist_width(0.0f, (float)width - 1.001f);
    std::uniform_real_distribution<float> dist_height(0.0f, (float)height - 1.001f);

    float inertia = 0.85f;
    float capacity_factor = 4.0f;
    float K_erode = 0.0025f;
    float K_dep = 0.0025f;
    float evap_rate = 0.01f;
    float gravity = 8.0f;

    auto get_height_and_gradient = [this](float x, float y, float& gx, float& gy) {
        int x0 = (int)x;
        int y0 = (int)y;
        float tx = x - x0;
        float ty = y - y0;
        
        float h00 = grid[y0 * width + x0].terrain_height();
        float h10 = grid[y0 * width + (x0 + 1)].terrain_height();
        float h01 = grid[(y0 + 1) * width + x0].terrain_height();
        float h11 = grid[(y0 + 1) * width + (x0 + 1)].terrain_height();
        
        gx = (h10 - h00) * (1.0f - ty) + (h11 - h01) * ty;
        gy = (h01 - h00) * (1.0f - tx) + (h11 - h10) * tx;
        
        return h00 * (1.0f - tx) * (1.0f - ty) +
               h10 * tx * (1.0f - ty) +
               h01 * (1.0f - tx) * ty +
               h11 * tx * ty;
    };

    auto get_height = [this](float x, float y) {
        int x0 = (int)x;
        int y0 = (int)y;
        float tx = x - x0;
        float ty = y - y0;
        
        float h00 = grid[y0 * width + x0].terrain_height();
        float h10 = grid[y0 * width + (x0 + 1)].terrain_height();
        float h01 = grid[(y0 + 1) * width + x0].terrain_height();
        float h11 = grid[(y0 + 1) * width + (x0 + 1)].terrain_height();
        
        return h00 * (1.0f - tx) * (1.0f - ty) +
               h10 * tx * (1.0f - ty) +
               h01 * (1.0f - tx) * ty +
               h11 * tx * ty;
    };

    for (int i = 0; i < num_droplets; ++i) {
        float x, y, vx, vy;
        if (scenario == "valley_test") {
            x = (width / 2.0f) + dist_jitter(rng);
            y = 1.0f + dist_jitter(rng);
            vx = 0.0f;
            vy = 0.5f;
        } else {
            x = dist_width(rng);
            y = dist_height(rng);
            vx = 0.0f;
            vy = 0.0f;
        }

        float water = 1.0f;
        float sediment = 0.0f;
        int last_x0 = -1, last_y0 = -1;

        for (int step = 0; step < 250; ++step) {
            int x0 = (int)x;
            int y0 = (int)y;
            if (x0 < 0 || x0 >= width - 1 || y0 < 0 || y0 >= height - 1) break;

            // Increment aggregate volumetric flow ONCE when crossing into a cell
            if (x0 != last_x0 || y0 != last_y0) {
                flow_vol[y0 * width + x0] += water;
                last_x0 = x0;
                last_y0 = y0;
            }

            float gx, gy;
            float h = get_height_and_gradient(x, y, gx, gy);

            // Accelerate downhill
            vx = vx * inertia - gx * gravity;
            vy = vy * inertia - gy * gravity;

            float speed = std::sqrt(vx * vx + vy * vy);
            if (speed < 0.05f) break; // Kill stagnant droplets early to prevent stagnation pile-ups

            float nx = x + vx;
            float ny = y + vy;
            int nx0 = (int)nx;
            int ny0 = (int)ny;
            if (nx0 < 0 || nx0 >= width - 1 || ny0 < 0 || ny0 >= height - 1) break;

            float h_next = get_height(nx, ny);
            float delta_h = h - h_next;

            // Clamped gradient slope to prevent carry capacity spikes
            float slope = std::clamp(delta_h, -0.5f, 0.5f);
            float c = std::max(0.0f, speed * water * slope * capacity_factor);

            // Bilinear weightings
            float tx = x - x0;
            float ty = y - y0;
            float w00 = (1.0f - tx) * (1.0f - ty);
            float w10 = tx * (1.0f - ty);
            float w01 = (1.0f - tx) * ty;
            float w11 = tx * ty;

            if (sediment < c) {
                // Erosion
                float amount = (c - sediment) * K_erode;
                int idxs[] = {y0 * width + x0, y0 * width + x0 + 1, (y0 + 1) * width + x0, (y0 + 1) * width + x0 + 1};
                float ws[] = {w00, w10, w01, w11};

                for (int n = 0; n < 4; ++n) {
                    float w = ws[n];
                    int nidx = idxs[n];
                    float eroded = std::min(grid[nidx].soil, amount * w);
                    grid[nidx].soil -= eroded;
                    sediment += eroded;

                    // Incise bedrock if soil is stripped (mass-conserved)
                    float remain = amount * w - eroded;
                    if (remain > 0.0f) {
                        float b_hardness = (scenario == "valley_test") ? 0.005f : 0.00001f;
                        float b_eroded = remain * b_hardness;
                        float old_b = grid[nidx].bedrock;
                        grid[nidx].bedrock = std::max(0.0f, old_b - b_eroded);
                        sediment += (old_b - grid[nidx].bedrock);
                    }
                }
            } else {
                // Deposition
                float amount = (sediment - c) * K_dep;
                int idxs[] = {y0 * width + x0, y0 * width + x0 + 1, (y0 + 1) * width + x0, (y0 + 1) * width + x0 + 1};
                float ws[] = {w00, w10, w01, w11};

                for (int n = 0; n < 4; ++n) {
                    float w = ws[n];
                    int nidx = idxs[n];
                    grid[nidx].soil += amount * w;
                    sediment -= amount * w;
                }
            }

            // Evaporation
            water *= (1.0f - evap_rate);

            x = nx;
            y = ny;
        }
        // We discard remaining sediment at death to represent suspended load leaving the system.
        // This prevents the formation of artificial sediment blockages at the outlet.
    }

    // 2. Convert Volumetric Flow to Manning Water Depth
    // Spring head discharge: 10 L/s = 864 m3/day
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float Q_day = flow_vol[idx] * (864.0f / (float)num_droplets);
            float Q_sec = Q_day / 86400.0f; // m3/s

            if (Q_sec > 0.0f) {
                float dz_dx = 0.5f * (grid[y*width + std::min(width-1, x+1)].terrain_height() - grid[y*width + std::max(0, x-1)].terrain_height());
                float dz_dy = 0.5f * (grid[std::min(height-1, y+1)*width + x].terrain_height() - grid[std::max(0, y-1)*width + x].terrain_height());
                float S = std::max(0.001f, std::sqrt(dz_dx*dz_dx + dz_dy*dz_dy));

                // Manning Depth (n = 0.04, width = 1.0m): d = (Q*n/sqrt(S))^0.6
                float depth = std::pow((Q_sec * 0.04f) / std::sqrt(S), 0.6f);
                grid[idx].water = std::max(0.02f, depth); // 2cm minimum thickness for rendering
            } else {
                grid[idx].water = 0.0f;
            }
        }
    }

    // 3. Gravity-Driven Soil Creep (Angle of repose smoothing)
    std::vector<float> soil_changes(width * height, 0.0f);
    const float creep_rate = 0.15f;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            int idx = y * width + x;
            float h_curr = grid[idx].terrain_height();

            for (int d = 0; d < 4; ++d) {
                int nidx = (y + dy[d]) * width + (x + dx[d]);
                float h_neigh = grid[nidx].terrain_height();
                
                if (h_curr > h_neigh + 0.1f) {
                    float diff = h_curr - h_neigh;
                    float move = (diff - 0.1f) * creep_rate * 0.25f;
                    move = std::min(move, grid[idx].soil * 0.25f);
                    soil_changes[idx] -= move;
                    soil_changes[nidx] += move;
                }
            }
        }
    }

    for (int i = 0; i < width * height; ++i) {
        grid[i].soil += soil_changes[i];
        grid[i].soil = std::max(0.0f, grid[i].soil);
    }

    // 4. Open Boundary Conditions
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                int idx = y * width + x;
                grid[idx].water = 0.0f;
                grid[idx].groundwater *= 0.5f;
                grid[idx].sediment = 0.0f;
            }
        }
    }
}

bool Simulator::save_layers_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "x,y,bedrock,soil,water,groundwater,sediment\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            file << x << "," << y << "," << cell.bedrock << "," << cell.soil << ","
                 << cell.water << "," << cell.groundwater << "," << cell.sediment << "\n";
        }
    }
    return true;
}

bool Simulator::save_top_view_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);

    // Flow accumulation calculated purely for standard scenery vegetation
    std::vector<float> flow_acc(width * height, 1.0f);
    std::vector<int> sorted_indices(width * height);
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::sort(sorted_indices.begin(), sorted_indices.end(), [this](int a, int b) {
        return grid[a].terrain_height() > grid[b].terrain_height();
    });

    int dx_d8[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy_d8[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    float dist_d8[] = {1.414f, 1.0f, 1.414f, 1.0f, 1.0f, 1.414f, 1.0f, 1.414f};

    for (int idx : sorted_indices) {
        int x = idx % width;
        int y = idx / width;
        float head_curr = grid[idx].terrain_height();
        int best_n = -1;
        float max_s = 0.0f;

        for (int d = 0; d < 8; ++d) {
            int nx = x + dx_d8[d];
            int ny = y + dy_d8[d];
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            int nidx = ny * width + nx;
            float head_neigh = grid[nidx].terrain_height();
            if (head_curr > head_neigh) {
                float slope = (head_curr - head_neigh) / dist_d8[d];
                if (slope > max_s) {
                    max_s = slope;
                    best_n = nidx;
                }
            }
        }
        if (best_n != -1) {
            flow_acc[best_n] += flow_acc[idx];
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            
            float h = cell.terrain_height();
            unsigned char r_land, g_land, b_land;
            
            if (cell.soil <= 0.005f) {
                // Bare granite bedrock
                r_land = 135; g_land = 125; b_land = 125;
            } else {
                // Soil with grass
                float norm_h = std::clamp((h - 10.0f) / 20.0f, 0.0f, 1.0f);
                if (norm_h < 0.5f) {
                    float t = norm_h / 0.5f;
                    r_land = (1.0f - t) * 60 + t * 110;
                    g_land = (1.0f - t) * 120 + t * 95;
                    b_land = (1.0f - t) * 60 + t * 50;
                } else {
                    float t = (norm_h - 0.5f) / 0.5f;
                    r_land = (1.0f - t) * 110 + t * 180;
                    g_land = (1.0f - t) * 95 + t * 180;
                    b_land = (1.0f - t) * 50 + t * 180;
                }
            }

            // Water rendering from physical depth (m)
            float water_depth = cell.water;
            float r_out = r_land;
            float g_out = g_land;
            float b_out = b_land;

            if (water_depth > 0.0f) {
                float alpha = 0.0f;
                if (water_depth < 0.05f) {
                    alpha = (water_depth / 0.05f) * 0.6f;
                } else {
                    alpha = 0.6f + (std::min(water_depth - 0.05f, 0.45f) / 0.45f) * 0.4f;
                }

                unsigned char r_water = 30;
                unsigned char g_water = 90;
                unsigned char b_water = 220;

                r_out = (1.0f - alpha) * r_land + alpha * r_water;
                g_out = (1.0f - alpha) * g_land + alpha * g_water;
                b_out = (1.0f - alpha) * b_land + alpha * b_water;
            }

            int pixel_idx = (y * width + x) * 3;
            pixels[pixel_idx] = r_out;
            pixels[pixel_idx + 1] = g_out;
            pixels[pixel_idx + 2] = b_out;
        }
    }

    int success = stbi_write_png(filepath.c_str(), width, height, 3, pixels.data(), width * 3);
    return success != 0;
}

bool Simulator::save_side_view_png(const std::string& filepath) const {
    int img_w = 512;
    int img_h = 512;
    std::vector<unsigned char> pixels(img_w * img_h * 3, 20);
    
    int slice_y = height * 3 / 4;
    int vy = std::clamp(slice_y, 0, height - 1);
    
    float min_h = 999.0f;
    float max_h = -999.0f;
    for (int px = 0; px < img_w; ++px) {
        int vx = px * width / img_w;
        vx = std::clamp(vx, 0, width - 1);
        const Cell& cell = grid[vy * width + vx];
        float h_water = cell.bedrock + cell.soil + cell.water;
        min_h = std::min(min_h, cell.bedrock);
        max_h = std::max(max_h, h_water);
    }
    
    float z_min = std::max(0.0f, min_h - 2.0f);
    float z_max = max_h + 2.0f;
    float z_range = std::max(5.0f, z_max - z_min);
    
    for (int py = 0; py < img_h; ++py) {
        float vz_float = z_min + (float)(img_h - 1 - py) * z_range / (float)img_h;
        
        for (int px = 0; px < img_w; ++px) {
            int vx = px * width / img_w;
            vx = std::clamp(vx, 0, width - 1);
            
            const Cell& cell = grid[vy * width + vx];
            float h_bedrock = cell.bedrock;
            float h_soil = h_bedrock + cell.soil;
            float water_depth = cell.water;
            
            float dz = z_range / (float)img_h;
            float v_water_top = h_soil + ((water_depth > 0.0f) ? std::max(water_depth, dz) : 0.0f);
            float v_soil_top = h_bedrock + ((cell.soil > 0.0f) ? std::max(cell.soil, dz) : 0.0f);
            float v_bedrock_top = h_bedrock;
            
            unsigned char r = 25, g = 35, b = 45;
            
            if (vz_float <= v_bedrock_top) {
                r = 135; g = 125; b = 125;
            } else if (vz_float <= v_soil_top) {
                if (vz_float > v_soil_top - dz * 1.5f) {
                    r = 75; g = 145; b = 60;
                } else {
                    r = 130; g = 95; b = 60;
                }
            } else if (vz_float <= v_water_top) {
                r = 30; g = 90; b = 220;
            }
            
            int pix_idx = (py * img_w + px) * 3;
            pixels[pix_idx] = r;
            pixels[pix_idx + 1] = g;
            pixels[pix_idx + 2] = b;
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), img_w, img_h, 3, pixels.data(), img_w * 3);
    return success != 0;
}

bool Simulator::save_iso_view_png(const std::string& filepath) const {
    int img_w = 512;
    int img_h = 512;
    std::vector<unsigned char> pixels(img_w * img_h * 3, 20);
    
    float dx_ray = -0.707f;
    float dy_ray = -0.707f;
    float dz_ray = -0.55f;
    float len = std::sqrt(dx_ray * dx_ray + dy_ray * dy_ray + dz_ray * dz_ray);
    dx_ray /= len; dy_ray /= len; dz_ray /= len;
    
    float rx = 0.707f; float ry = -0.707f; float rz = 0.0f;
    float ux = -0.38f; float uy = -0.38f; float uz = 0.85f;
    
    float wx_c = (float)width / 2.0f;
    float wy_c = (float)height / 2.0f;
    float wz_c = 15.0f;
    
    float scale = 0.6f * ((float)width / 256.0f);
    float cam_dist = 180.0f * ((float)width / 256.0f);
    
    for (int py = 0; py < img_h; ++py) {
        for (int px = 0; px < img_w; ++px) {
            float ox = wx_c + ((float)px - 256.0f) * rx * scale + (256.0f - (float)py) * ux * scale - cam_dist * dx_ray;
            float oy = wy_c + ((float)px - 256.0f) * ry * scale + (256.0f - (float)py) * uy * scale - cam_dist * dy_ray;
            float oz = wz_c + ((float)px - 256.0f) * rz * scale + (256.0f - (float)py) * uz * scale - cam_dist * dz_ray;
            
            float r = 25.0f, g = 35.0f, b = 45.0f;
            
            float t = 0.0f;
            float t_max = 2.0f * (float)width;
            float dt_step = 0.2f;
            
            while (t < t_max) {
                float cx = ox + t * dx_ray;
                float cy = oy + t * dy_ray;
                float cz = oz + t * dz_ray;
                
                int vx = (int)std::floor(cx);
                int vy = (int)std::floor(cy);
                
                if (vx >= 0 && vx < width && vy >= 0 && vy < height) {
                    int cell_idx = vy * width + vx;
                    const Cell& cell = grid[cell_idx];
                    
                    float h_bedrock = cell.bedrock;
                    float h_soil = h_bedrock + cell.soil;
                    float water_depth = cell.water;
                    float h_water = h_soil + water_depth;
                    
                    if (cz <= h_water) {
                        float shade = 0.65f;
                        if (cz > h_water - 0.1f) shade = 1.0f;
                        
                        if (cz <= h_bedrock) {
                            r = 135.0f; g = 125.0f; b = 125.0f;
                        } else if (cz <= h_soil) {
                            if (cz > h_soil - 0.2f) {
                                r = 75.0f; g = 145.0f; b = 60.0f;
                            } else {
                                r = 130.0f; g = 95.0f; b = 60.0f;
                            }
                        } else {
                            r = 30.0f; g = 90.0f; b = 220.0f;
                            shade = 0.9f;
                        }
                        
                        r *= shade; g *= shade; b *= shade;
                        break;
                    }
                }
                t += dt_step;
            }
            
            int pix_idx = (py * img_w + px) * 3;
            pixels[pix_idx] = (unsigned char)std::clamp(r, 0.0f, 255.0f);
            pixels[pix_idx + 1] = (unsigned char)std::clamp(g, 0.0f, 255.0f);
            pixels[pix_idx + 2] = (unsigned char)std::clamp(b, 0.0f, 255.0f);
        }
    }
    
    int success = stbi_write_png(filepath.c_str(), img_w, img_h, 3, pixels.data(), img_w * 3);
    return success != 0;
}

} // namespace sim_rivers
} // namespace jotcad
