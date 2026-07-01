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

// 2D Vector structure for droplet movement
struct Vector2 {
    float x;
    float y;
    Vector2(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}
    void multiplyScalar(float s) {
        x *= s;
        y *= s;
    }
};

// 3D Vector structure for surface normals
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

// Helper for central-difference normal
static Vector3 centralDiffNormal(const std::vector<float>& heightMap, int width, int height, int i, int j, float verticalScale) {
    int ix0 = std::max(1, std::min(width - 2, i));
    int jy0 = std::max(1, std::min(height - 2, j));
    auto idx = [width](int x, int y) { return y * width + x; };

    float dzdx = ((heightMap[idx(ix0 + 1, jy0)] - heightMap[idx(ix0 - 1, jy0)]) * 0.5f) * verticalScale;
    float dzdy = ((heightMap[idx(ix0, jy0 + 1)] - heightMap[idx(ix0, jy0 - 1)]) * 0.5f) * verticalScale;

    return Vector3(-dzdx, 1.0f, -dzdy).normalize();
}

// Compute the surface normal using weighted facets (Tessa's method)
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

    // +X facet
    n.add(Vector3(verticalScale * (h00 - hpx), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));

    // -X facet
    n.add(Vector3(verticalScale * (hnx - h00), 1.0f, 0.0f).normalize().multiplyScalar(cardinalWeight));

    // +Y facet
    n.add(Vector3(0.0f, 1.0f, verticalScale * (h00 - hpy)).normalize().multiplyScalar(cardinalWeight));

    // -Y facet
    n.add(Vector3(0.0f, 1.0f, verticalScale * (hny - h00)).normalize().multiplyScalar(cardinalWeight));

    // Diagonals
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
                // Flat-walled ramp: valley floor is flat from x=2 to x=width-3, flanked by high 10m narrow walls
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
                cell.soil = 20.0f + noise + rand_noise;
            } else {
                // Standard terrain: y-axis slope + Perlin noise
                float nx = (float)x / (width - 1);
                float ny = (float)y / (height - 1);
                float slope = (1.0f - ny) * 10.0f;
                float noise = noiseGen.fBm(nx * 3.0f, ny * 3.0f, 4, 2.0, 0.5) * 4.0f;
                cell.bedrock = 10.0f + slope;
                cell.soil = 2.0f + noise;
            }
        }
    }
}

void Simulator::step(float dt) {
    step_count++;
    (void)dt;

    // Number of droplets to simulate per step
    // 200 steps * 1000 droplets = 200,000 total iterations (matching Tessa's default)
    int num_droplets = 1000;
    std::vector<float> flow_vol(width * height, 0.0f);

    std::uniform_real_distribution<float> dist_x(0.0f, (float)width - 1.001f);
    std::uniform_real_distribution<float> dist_y(0.0f, (float)height - 1.001f);

    // Tessa's parameters
    const float timeStep = 1.2f;
    const float density = 1.0f;
    const float evaporationRate = 0.001f;
    const float depositionRate = 0.1f;
    const float minVolume = 0.01f;
    const float friction = 0.05f;
    const float heightScale = 1.0f;

    // Build temporary 1D heightmap for normal computation
    std::vector<float> heightMap(width * height);
    for (int i = 0; i < width * height; ++i) {
        heightMap[i] = grid[i].terrain_height();
    }

    for (int d = 0; d < num_droplets; ++d) {
        // Initialize droplet at random position
        Vector2 pos(dist_x(rng), dist_y(rng));
        Vector2 direction(0.0f, 0.0f);
        float volume = 1.0f;
        float sediment = 0.0f;

        while (volume > minVolume) {
            int x = (int)std::floor(pos.x);
            int y = (int)std::floor(pos.y);

            if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1) {
                break;
            }

            // Track flow volume for rendering
            flow_vol[y * width + x] += volume;

            // Compute surface normal
            Vector3 normal = surfaceNormal(heightMap, width, height, x, y, heightScale);

            // Accelerate by slope, scaled by mass
            float inverseMass = 1.0f / (volume * density);
            direction.x += timeStep * normal.x * inverseMass;
            direction.y += timeStep * normal.z * inverseMass;

            // Integrate position
            pos.x += timeStep * direction.x;
            pos.y += timeStep * direction.y;

            // Apply friction
            direction.multiplyScalar(1.0f - timeStep * friction);

            // Bounds check
            if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) {
                break;
            }

            int nx = (int)std::floor(pos.x);
            int ny = (int)std::floor(pos.y);
            float startHeight = heightMap[y * width + x];
            float nextHeight = heightMap[ny * width + nx];

            // Calculate sediment capacity
            float speed = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            float capacity = volume * speed * (startHeight - nextHeight);
            capacity = std::max(0.0f, capacity);

            float capacityDelta = capacity - sediment;

            // Update sediment
            float deltaSediment = timeStep * depositionRate * capacityDelta;
            sediment += deltaSediment;

            // Update terrain (both local heightMap and grid values)
            float deltaHeight = timeStep * volume * depositionRate * capacityDelta;
            heightMap[y * width + x] -= deltaHeight;

            // Modify soil/bedrock layer representation in JotCAD grid
            if (deltaHeight > 0.0f) {
                // Erosion: take from soil first
                float eroded = std::min(grid[y * width + x].soil, deltaHeight);
                grid[y * width + x].soil -= eroded;
                float remain = deltaHeight - eroded;
                if (remain > 0.0f) {
                    grid[y * width + x].bedrock = std::max(0.0f, grid[y * width + x].bedrock - remain);
                }
            } else {
                // Deposition: add to soil
                grid[y * width + x].soil += (-deltaHeight);
            }

            // Evaporate
            volume *= (1.0f - timeStep * evaporationRate);
        }
    }

    // Convert flow volume to water depth for top/side/iso rendering
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float Q_day = flow_vol[idx] * (864.0f / (float)num_droplets);
            float Q_sec = Q_day / 86400.0f; // m3/s

            if (Q_sec > 0.0f) {
                float dz_dx = 0.5f * (grid[y*width + std::min(width-1, x+1)].terrain_height() - grid[y*width + std::max(0, x-1)].terrain_height());
                float dz_dy = 0.5f * (grid[std::min(height-1, y+1)*width + x].terrain_height() - grid[std::max(0, y-1)*width + x].terrain_height());
                float S = std::max(0.001f, std::sqrt(dz_dx*dz_dx + dz_dy*dz_dy));

                // Manning Depth formula for water rendering
                float depth = std::pow((Q_sec * 0.04f) / std::sqrt(S), 0.6f);
                grid[idx].water = std::max(0.02f, depth);
            } else {
                grid[idx].water = 0.0f;
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
                r_land = 135; g_land = 125; b_land = 125;
            } else {
                unsigned char r_sed = 170;
                unsigned char g_sed = 150;
                unsigned char b_sed = 130;

                float f_acc = flow_acc[idx];
                if (f_acc > 12.0f || cell.water > 0.0f) {
                    r_land = r_sed;
                    g_land = g_sed;
                    b_land = b_sed;
                } else {
                    float norm_h = std::clamp((h - 10.0f) / 20.0f, 0.0f, 1.0f);
                    unsigned char r_grass, g_grass, b_grass;
                    if (norm_h < 0.5f) {
                        float t = norm_h / 0.5f;
                        r_grass = (1.0f - t) * 60 + t * 110;
                        g_grass = (1.0f - t) * 120 + t * 95;
                        b_grass = (1.0f - t) * 60 + t * 50;
                    } else {
                        float t = (norm_h - 0.5f) / 0.5f;
                        r_grass = (1.0f - t) * 110 + t * 180;
                        g_grass = (1.0f - t) * 95 + t * 180;
                        b_grass = (1.0f - t) * 50 + t * 180;
                    }
                    r_land = 0.3f * r_sed + 0.7f * r_grass;
                    g_land = 0.3f * g_sed + 0.7f * g_grass;
                    b_land = 0.3f * b_sed + 0.7f * b_grass;
                }
            }

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
    
    float sum_surface = 0.0f;
    for (int px = 0; px < img_w; ++px) {
        int vx = px * width / img_w;
        vx = std::clamp(vx, 0, width - 1);
        const Cell& cell = grid[vy * width + vx];
        sum_surface += cell.bedrock + cell.soil;
    }
    float surface_avg = sum_surface / (float)img_w;
    
    float z_min = surface_avg - 2.0f;
    float z_max = surface_avg + 1.0f;
    float z_range = 3.0f;
    
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
                unsigned char r_sed = 170;
                unsigned char g_sed = 150;
                unsigned char b_sed = 130;

                if (vz_float > v_soil_top - dz * 1.5f && water_depth <= 0.0f) {
                    unsigned char r_grass = 75; unsigned char g_grass = 145; unsigned char b_grass = 60;
                    r = 0.3f * r_sed + 0.7f * r_grass;
                    g = 0.3f * g_sed + 0.7f * g_grass;
                    b = 0.3f * b_sed + 0.7f * b_grass;
                } else {
                    r = r_sed;
                    g = g_sed;
                    b = b_sed;
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
                    float h_soil = h_bedrock + cell.soil;
                    float water_depth = cell.water;
                    float h_water = h_soil + water_depth;
                    
                    if (cz <= h_water) {
                        float shade = 0.65f;
                        if (cz > h_water - 0.1f) shade = 1.0f;
                        
                        if (cz <= h_bedrock) {
                            r = 135.0f; g = 125.0f; b = 125.0f;
                        } else if (cz <= h_soil) {
                            float r_sed = 170.0f;
                            float g_sed = 150.0f;
                            float b_sed = 130.0f;

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
