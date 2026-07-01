#include "simulator.h"
#include "noise.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <iostream>
#include <random>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace sim_rivers {

static PerlinNoise noiseGen;

struct Vector2 {
    float x;
    float y;
    Vector2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    void multiplyScalar(float s) {
        x *= s;
        y *= s;
    }
};

struct Vector3 {
    float x;
    float y;
    float z;
    Vector3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}
    Vector3& add(const Vector3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    Vector3& multiplyScalar(float s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    Vector3 normalize() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0.00001f) {
            return Vector3(x / len, y / len, z / len);
        }
        return Vector3(0, 0, 0);
    }
};

static Vector3 centralDiffNormal(const std::vector<float>& heightMap, int width, int height, int i, int j, float verticalScale) {
    int ix0 = std::max(1, std::min(width - 2, i));
    int jy0 = std::max(1, std::min(height - 2, j));
    auto idx = [width](int x, int y) { return y * width + x; };

    float dzdx = ((heightMap[idx(ix0 + 1, jy0)] - heightMap[idx(ix0 - 1, jy0)]) * 0.5f) * verticalScale;
    float dzdy = ((heightMap[idx(ix0, jy0 + 1)] - heightMap[idx(ix0, jy0 - 1)]) * 0.5f) * verticalScale;

    return Vector3(-dzdx, 1.0f, -dzdy).normalize();
}

static Vector3 surfaceNormal(const std::vector<float>& heightMap, int width, int height, int i, int j, float verticalScale) {
    if (i <= 0 || i >= width - 1 || j <= 0 || j >= height - 1) {
        return centralDiffNormal(heightMap, width, height, i, j, verticalScale);
    }

    auto idx = [width](int x, int y) { return y * width + x; };

    float h00 = heightMap[idx(i, j)];

    // Cardinals
    float hpx = heightMap[idx(i + 1, j)];
    float hnx = heightMap[idx(i - 1, j)];
    float hpy = heightMap[idx(i, j + 1)];
    float hny = heightMap[idx(i, j - 1)];
    // Diagonals
    float hpp = heightMap[idx(i + 1, j + 1)];
    float hpn = heightMap[idx(i + 1, j - 1)];
    float hnp = heightMap[idx(i - 1, j + 1)];
    float hnn = heightMap[idx(i - 1, j - 1)];

    const float cardinalWeight = 0.15f;
    const float diagonalWeight = 0.10f;
    const float root2 = 1.41421356f;

    Vector3 n(0, 0, 0);

    n.add(Vector3(verticalScale * (h00 - hpx), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(verticalScale * (hnx - h00), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(0.0f, 1.0f, verticalScale * (h00 - hpy)).normalize().multiplyScalar(cardinalWeight));
    n.add(Vector3(0.0f, 1.0f, verticalScale * (hny - h00)).normalize().multiplyScalar(cardinalWeight));

    n.add(Vector3(verticalScale * (h00 - hpp) / root2, root2, verticalScale * (h00 - hpp) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hpn) / root2, root2, verticalScale * (h00 - hpn) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hnp) / root2, root2, verticalScale * (h00 - hnp) / root2).normalize().multiplyScalar(diagonalWeight));
    n.add(Vector3(verticalScale * (h00 - hnn) / root2, root2, verticalScale * (h00 - hnn) / root2).normalize().multiplyScalar(diagonalWeight));

    return n;
}

Simulator::Simulator(int w, int h)
    : width(w), height(h), scenario("standard"), step_count(0), seed(1337) {
    grid.resize(width * height);
}

Simulator::~Simulator() {}

static void fill_depressions(std::vector<Cell>& grid, int width, int height) {
    std::vector<float> h(width * height);
    for (int i = 0; i < width * height; ++i) {
        h[i] = grid[i].terrain_height();
    }

    // Initialize non-boundaries to infinity
    std::vector<float> f(width * height, 999999.0f);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                f[idx] = h[idx];
            }
        }
    }

    // Fast sweep iterative propagation
    bool changed = true;
    int dx[] = {-1, 1, 0, 0, -1, 1, -1, 1};
    int dy[] = {0, 0, -1, 1, -1, -1, 1, 1};

    while (changed) {
        changed = false;
        // Forward pass
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = y * width + x;
                float min_n = 999999.0f;
                for (int d = 0; d < 8; ++d) {
                    min_n = std::min(min_n, f[(y + dy[d]) * width + (x + dx[d])]);
                }
                float val = std::max(h[idx], min_n + 0.001f);
                if (f[idx] > val) {
                    f[idx] = val;
                    changed = true;
                }
            }
        }
        // Backward pass
        for (int y = height - 2; y >= 1; --y) {
            for (int x = width - 2; x >= 1; --x) {
                int idx = y * width + x;
                float min_n = 999999.0f;
                for (int d = 0; d < 8; ++d) {
                    min_n = std::min(min_n, f[(y + dy[d]) * width + (x + dx[d])]);
                }
                float val = std::max(h[idx], min_n + 0.001f);
                if (f[idx] > val) {
                    f[idx] = val;
                    changed = true;
                }
            }
        }
    }

    // Apply back by filling sinks with clay and gravel split
    for (int i = 0; i < width * height; ++i) {
        float diff = f[i] - grid[i].terrain_height();
        if (diff > 0.0f) {
            grid[i].soil_clay += diff * 0.5f;
            grid[i].soil_gravel += diff * 0.5f;
            grid[i].soil = grid[i].soil_clay + grid[i].soil_gravel;
        }
    }
}

void Simulator::initialize(unsigned int seed_val) {
    seed = seed_val;
    rng.seed(seed);
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
                float v_slope = 0.0f;
                if (x <= 1 || x >= width - 2) {
                    v_slope = 10.0f;
                }
                float y_slope = (float)(height - 1 - y) * (0.8f / 63.0f);
                float nx = (float)x / (width - 1);
                float ny = (float)y / (height - 1);
                float noise = noiseGen.fBm(nx * 4.0f, ny * 4.0f, 3, 2.0, 0.5f) * 0.015f;
                
                std::uniform_real_distribution<float> dist_noise(-0.05f, 0.05f);
                float rand_noise = dist_noise(rng);
                
                cell.bedrock = 1.0f + v_slope + y_slope;
                float total_soil = 20.0f + noise + rand_noise;
                cell.soil_clay = total_soil * 0.5f;
                cell.soil_gravel = total_soil * 0.5f;
                cell.soil = total_soil;
            } else {
                float nx = (float)x / (width - 1);
                float ny = (float)y / (height - 1);
                float slope = (1.0f - ny) * 15.0f;
                float noise = noiseGen.fBm(nx * 3.0f, ny * 3.0f, 4, 2.0, 0.5) * 8.0f;

                cell.bedrock = 1.0f;
                float total_soil = std::max(5.0f, 11.0f + slope + noise);
                cell.soil_clay = total_soil * 0.5f;
                cell.soil_gravel = total_soil * 0.5f;
                cell.soil = total_soil;
            }
        }
    }

    // Fill closed depressions to ensure continuous drainage to the boundaries
    fill_depressions(grid, width, height);
}

void Simulator::step(float dt) {
    step_count++;
    (void)dt;

    int num_droplets = 1000;
    std::vector<float> flow_vol(width * height, 0.0f);

    std::uniform_real_distribution<float> dist_x(0.0f, (float)width - 1.001f);
    std::uniform_real_distribution<float> dist_y(0.0f, (float)height - 1.001f);
    std::uniform_real_distribution<float> dist_jitter(-0.4f, 0.4f);

    // Simulation parameters
    const float timeStep = 1.2f;
    const float density = 1.0f;
    const float evaporationRate = 0.001f;
    const float minVolume = 0.01f;
    
    const float K_clay = 0.1f;
    const float K_gravel = 0.1f;
    
    // Upstream flow curvature lag (inertia 0.85-0.90, set friction=0.10 so 1.0-dt*fric = 0.88)
    const float friction = 0.10f;
    const float heightScale = 1.0f;
    const float gravity = 8.0f; // Scale velocity to match physical ranges

    // Build vegetation map based on water presence (stable soil grows roots)
    std::vector<float> veg(width * height, 1.0f);
    for (int i = 0; i < width * height; ++i) {
        if (grid[i].water > 0.02f) {
            veg[i] = 0.0f; // washed away or drowned
        } else {
            veg[i] = 1.0f; // fully vegetated
        }
    }

    std::vector<float> heightMap(width * height);
    for (int i = 0; i < width * height; ++i) {
        heightMap[i] = grid[i].terrain_height();
    }

    for (int d = 0; d < num_droplets; ++d) {
        Vector2 pos(0.0f, 0.0f);
        Vector2 direction(0.0f, 0.0f);
        if (scenario == "valley_test") {
            pos.x = (width / 2.0f) + dist_jitter(rng);
            pos.y = 1.0f + dist_jitter(rng);
            direction.y = 0.5f;
        } else {
            pos.x = dist_x(rng);
            pos.y = dist_y(rng);
        }

        float volume = 1.0f;
        float sediment_clay = 0.0f;
        float sediment_gravel = 0.0f;
        int step_idx = 0;

        while (volume > minVolume) {
            int x = (int)std::floor(pos.x);
            int y = (int)std::floor(pos.y);

            if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1) {
                break;
            }

            flow_vol[y * width + x] += volume;

            // Compute surface normal
            Vector3 normal = surfaceNormal(heightMap, width, height, x, y, heightScale);

            // Update velocity scaled by gravity acceleration
            float inverseMass = 1.0f / (volume * density);
            direction.x += timeStep * normal.x * gravity * inverseMass;
            direction.y += timeStep * normal.z * gravity * inverseMass;

            // Integrate position
            pos.x += timeStep * direction.x;
            pos.y += timeStep * direction.y;

            // Apply friction (inertia lag)
            direction.multiplyScalar(1.0f - timeStep * friction);

            // Bounds check
            if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) {
                break;
            }

            int nx = (int)std::floor(pos.x);
            int ny = (int)std::floor(pos.y);
            float startHeight = heightMap[y * width + x];
            float nextHeight = heightMap[ny * width + nx];

            // Calculate total sediment capacity
            float speed = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            float capacity = volume * speed * (startHeight - nextHeight);
            capacity = std::max(0.0f, capacity);

            // Critical shear velocity & gravel mobility threshold
            float gravel_mobility = 0.0f;
            if (speed > 0.6f) {
                gravel_mobility = std::clamp((speed - 0.6f) / 0.2f, 0.0f, 1.0f);
            }

            // Capacity split
            float capacity_clay = capacity * 0.5f;
            float capacity_gravel = capacity * 0.5f * gravel_mobility;

            // Constant sediment feed
            if (step_idx == 0 && scenario == "valley_test") {
                sediment_clay = capacity_clay * 0.8f;
                sediment_gravel = capacity_gravel * 0.8f;
            }

            // Non-erodible boundaries
            if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                int cell_idx = y * width + x;
                float cell_veg = veg[cell_idx];

                // 1. Clay erosion/deposition (scaled by volume for mass conservation, with veg shielding)
                float delta_clay = timeStep * K_clay * (capacity_clay - sediment_clay);
                if (delta_clay > 0.0f) {
                    delta_clay *= (1.0f - 0.9f * cell_veg); // 90% erosion reduction on vegetated land
                }
                float terrain_delta_clay = delta_clay * volume;
                if (terrain_delta_clay > 0.0f) {
                    float eroded = std::min(grid[cell_idx].soil_clay, terrain_delta_clay);
                    grid[cell_idx].soil_clay -= eroded;
                    sediment_clay += eroded / volume;
                    
                    float remain = terrain_delta_clay - eroded;
                    if (remain > 0.0f) {
                        float b_eroded = remain * 0.005f;
                        grid[cell_idx].bedrock = std::max(0.0f, grid[cell_idx].bedrock - b_eroded);
                        sediment_clay += b_eroded / volume;
                    }
                } else {
                    float deposit = -terrain_delta_clay;
                    grid[cell_idx].soil_clay += deposit;
                    sediment_clay -= deposit / volume;
                }

                // 2. Gravel erosion/deposition (scaled by volume for mass conservation, with veg shielding)
                float delta_gravel = timeStep * K_gravel * (capacity_gravel - sediment_gravel);
                if (delta_gravel > 0.0f) {
                    delta_gravel *= (1.0f - 0.9f * cell_veg); // 90% erosion reduction on vegetated land
                }
                float terrain_delta_gravel = delta_gravel * volume;
                if (terrain_delta_gravel > 0.0f) {
                    float eroded = std::min(grid[cell_idx].soil_gravel, terrain_delta_gravel);
                    grid[cell_idx].soil_gravel -= eroded;
                    sediment_gravel += eroded / volume;
                    
                    float remain = terrain_delta_gravel - eroded;
                    if (remain > 0.0f) {
                        float b_eroded = remain * 0.005f;
                        grid[cell_idx].bedrock = std::max(0.0f, grid[cell_idx].bedrock - b_eroded);
                        sediment_gravel += b_eroded / volume;
                    }
                } else {
                    float deposit = -terrain_delta_gravel;
                    grid[cell_idx].soil_gravel += deposit;
                    sediment_gravel -= deposit / volume;
                }

                // Sync grid.soil and heightMap
                grid[cell_idx].soil = grid[cell_idx].soil_clay + grid[cell_idx].soil_gravel;
                heightMap[cell_idx] = grid[cell_idx].terrain_height();
            }

            // Evaporate
            volume *= (1.0f - timeStep * evaporationRate);
            step_idx++;
        }
    }

    // Convert flow volume to water depth
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float Q_day = flow_vol[idx] * (864.0f / (float)num_droplets);
            float Q_sec = Q_day / 86400.0f; // m3/s

            if (Q_sec > 0.0f) {
                float dz_dx = 0.5f * (grid[y*width + std::min(width-1, x+1)].terrain_height() - grid[y*width + std::max(0, x-1)].terrain_height());
                float dz_dy = 0.5f * (grid[std::min(height-1, y+1)*width + x].terrain_height() - grid[std::max(0, y-1)*width + x].terrain_height());
                float S = std::max(0.001f, std::sqrt(dz_dx*dz_dx + dz_dy*dz_dy));

                float depth = std::pow((Q_sec * 0.04f) / std::sqrt(S), 0.6f);
                grid[idx].water = std::max(0.02f, depth);
            } else {
                grid[idx].water = 0.0f;
            }
        }
    }

    // Cohesive vs. Non-Cohesive Gravity-Driven Soil Creep (River Bank Formation)
    const float creep_rate = 0.25f;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int iter = 0; iter < 5; ++iter) {
        std::vector<float> clay_changes(width * height, 0.0f);
        std::vector<float> gravel_changes(width * height, 0.0f);

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = y * width + x;
                const Cell& cell = grid[idx];
                float total_soil = cell.soil_clay + cell.soil_gravel;
                if (total_soil <= 0.005f) continue;

                float h_curr = cell.terrain_height();
                float clay_ratio = cell.soil_clay / (total_soil + 1e-6f);

                // Cohesive angle of repose threshold: Clay stands steeper (0.30f) than Gravel (0.10f)
                float slope_threshold = 0.10f + clay_ratio * 0.20f;

                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    int nidx = ny * width + nx;
                    float h_neigh = grid[nidx].terrain_height();

                    if (h_curr > h_neigh + slope_threshold) {
                        float diff = h_curr - h_neigh;
                        float local_creep = creep_rate;
                        if (grid[nidx].water > 0.02f) {
                            local_creep *= 3.0f; // Bank undercutting triggers collapse
                        } else {
                            // Scale down creep by 95% on vegetated stable land to prevent general flattening
                            local_creep *= (1.0f - 0.95f * veg[idx]);
                        }
                        
                        if (local_creep <= 0.0001f) continue;

                        float move = (diff - slope_threshold) * local_creep * 0.25f;
                        move = std::min(move, total_soil * 0.10f); // limit per iteration to maintain numerical stability

                        clay_changes[idx] -= move * clay_ratio;
                        clay_changes[nidx] += move * clay_ratio;

                        gravel_changes[idx] -= move * (1.0f - clay_ratio);
                        gravel_changes[nidx] += move * (1.0f - clay_ratio);
                    }
                }
            }
        }

        // Apply creep changes and sync grid.soil for next iteration
        for (int idx = 0; idx < width * height; ++idx) {
            grid[idx].soil_clay = std::max(0.0f, grid[idx].soil_clay + clay_changes[idx]);
            grid[idx].soil_gravel = std::max(0.0f, grid[idx].soil_gravel + gravel_changes[idx]);
            grid[idx].soil = grid[idx].soil_clay + grid[idx].soil_gravel;
        }
    }
}

bool Simulator::save_layers_csv(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "x,y,bedrock,soil,soil_clay,soil_gravel,water,groundwater,sediment\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            const Cell& cell = grid[idx];
            file << x << "," << y << "," << cell.bedrock << "," << cell.soil << ","
                 << cell.soil_clay << "," << cell.soil_gravel << ","
                 << cell.water << "," << cell.groundwater << "," << cell.sediment << "\n";
        }
    }
    return true;
}

bool Simulator::save_top_view_png(const std::string& filepath) const {
    std::vector<unsigned char> pixels(width * height * 3);
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
            
            unsigned char r_land, g_land, b_land;
            float total_soil = cell.soil_clay + cell.soil_gravel;
            
            if (total_soil <= 0.005f) {
                r_land = 135; g_land = 125; b_land = 125;
            } else {
                float clay_ratio = cell.soil_clay / (total_soil + 1e-6f);
                float gravel_ratio = cell.soil_gravel / (total_soil + 1e-6f);

                float r_clay = 130.0f, g_clay = 75.0f, b_clay = 55.0f;
                float r_gravel = 190.0f, g_gravel = 175.0f, b_gravel = 155.0f;

                float r_sed = clay_ratio * r_clay + gravel_ratio * r_gravel;
                float g_sed = clay_ratio * g_clay + gravel_ratio * g_gravel;
                float b_sed = clay_ratio * b_clay + gravel_ratio * b_gravel;

                float r_grass_base = 75.0f, g_grass_base = 145.0f, b_grass_base = 60.0f;
                float r_grass = r_grass_base * 0.8f + r_sed * 0.2f;
                float g_grass = g_grass_base * 0.8f + g_sed * 0.2f;
                float b_grass = b_grass_base * 0.8f + b_sed * 0.2f;

                float f_acc = flow_acc[idx];
                if (f_acc > 12.0f || cell.water > 0.0f) {
                    r_land = (unsigned char)std::clamp(r_sed, 0.0f, 255.0f);
                    g_land = (unsigned char)std::clamp(g_sed, 0.0f, 255.0f);
                    b_land = (unsigned char)std::clamp(b_sed, 0.0f, 255.0f);
                } else {
                    r_land = (unsigned char)std::clamp(r_grass, 0.0f, 255.0f);
                    g_land = (unsigned char)std::clamp(g_grass, 0.0f, 255.0f);
                    b_land = (unsigned char)std::clamp(b_grass, 0.0f, 255.0f);
                }
            }

            float water_depth = cell.water;
            float r_out = r_land;
            float g_out = g_land;
            float b_out = b_land;

            if (water_depth > 0.0f) {
                float alpha = 0.85f + std::min(water_depth * 2.0f, 0.15f);

                unsigned char r_water = 15;
                unsigned char g_water = 60;
                unsigned char b_water = 245;

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
        float h_water = cell.bedrock + cell.soil_clay + cell.soil_gravel + cell.water;
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
            float total_soil = cell.soil_clay + cell.soil_gravel;
            float h_soil = h_bedrock + total_soil;
            float water_depth = cell.water;
            
            float dz = z_range / (float)img_h;
            float v_water_top = h_soil + ((water_depth > 0.0f) ? std::max(water_depth * 15.0f, 0.35f) : 0.0f);
            float v_soil_top = h_bedrock + ((total_soil > 0.0f) ? std::max(total_soil, dz) : 0.0f);
            float v_bedrock_top = h_bedrock;
            
            unsigned char r = 25, g = 35, b = 45;
            
            if (vz_float <= v_bedrock_top) {
                r = 135; g = 125; b = 125;
            } else if (vz_float <= v_soil_top) {
                float clay_ratio = cell.soil_clay / (total_soil + 1e-6f);
                float gravel_ratio = cell.soil_gravel / (total_soil + 1e-6f);
                float r_sed = clay_ratio * 130.0f + gravel_ratio * 190.0f;
                float g_sed = clay_ratio * 75.0f + gravel_ratio * 175.0f;
                float b_sed = clay_ratio * 55.0f + gravel_ratio * 155.0f;

                if (vz_float > v_soil_top - dz * 1.5f && water_depth <= 0.0f) {
                    unsigned char r_grass = 75; unsigned char g_grass = 145; unsigned char b_grass = 60;
                    r = 0.3f * r_sed + 0.7f * r_grass;
                    g = 0.3f * g_sed + 0.7f * g_grass;
                    b = 0.3f * b_sed + 0.7f * b_grass;
                } else {
                    r = (unsigned char)std::clamp(r_sed, 0.0f, 255.0f);
                    g = (unsigned char)std::clamp(g_sed, 0.0f, 255.0f);
                    b = (unsigned char)std::clamp(b_sed, 0.0f, 255.0f);
                }
            } else if (vz_float <= v_water_top) {
                r = 15; g = 60; b = 245;
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
    
    float max_dim = (float)std::max(width, height);
    float scale = 0.6f * (max_dim / 256.0f);
    float cam_dist = 180.0f * (max_dim / 256.0f);
    
    for (int py = 0; py < img_h; ++py) {
        for (int px = 0; px < img_w; ++px) {
            float ox = wx_c + ((float)px - 256.0f) * rx * scale + (256.0f - (float)py) * ux * scale - cam_dist * dx_ray;
            float oy = wy_c + ((float)px - 256.0f) * ry * scale + (256.0f - (float)py) * uy * scale - cam_dist * dy_ray;
            float oz = wz_c + ((float)px - 256.0f) * rz * scale + (256.0f - (float)py) * uz * scale - cam_dist * dz_ray;
            
            float r = 25.0f, g = 35.0f, b = 45.0f;
            
            float t = 0.0f;
            float t_max = 2.0f * max_dim;
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
                    float total_soil = cell.soil_clay + cell.soil_gravel;
                    float h_soil = h_bedrock + total_soil;
                    float water_depth = cell.water;
                    float h_water = h_soil + ((water_depth > 0.0f) ? std::max(water_depth * 15.0f, 0.35f) : 0.0f);
                    
                    if (cz <= h_water) {
                        float shade = 0.65f;
                        if (cz > h_water - 0.1f) shade = 1.0f;
                        
                        if (cz <= h_bedrock) {
                            r = 135.0f; g = 125.0f; b = 125.0f;
                        } else if (cz <= h_soil) {
                            float clay_ratio = cell.soil_clay / (total_soil + 1e-6f);
                            float gravel_ratio = cell.soil_gravel / (total_soil + 1e-6f);
                            float r_sed = clay_ratio * 130.0f + gravel_ratio * 190.0f;
                            float g_sed = clay_ratio * 75.0f + gravel_ratio * 175.0f;
                            float b_sed = clay_ratio * 55.0f + gravel_ratio * 155.0f;

                            if (cz > h_soil - 0.2f && water_depth <= 0.0f) {
                                float r_grass = 75.0f; float g_grass = 145.0f; float b_grass = 60.0f;
                                r = 0.3f * r_sed + 0.7f * r_grass;
                                g = 0.3f * g_sed + 0.7f * g_grass;
                                b = 0.3f * b_sed + 0.7f * b_grass;
                            } else {
                                r = r_sed;
                                g = g_sed;
                                b = b_sed;
                            }
                        } else {
                            r = 15.0f; g = 60.0f; b = 245.0f;
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
