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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../fs/cpp/vendor/stb_image_write.h"

void save_png(const std::string& filename, Grid& g) {
    int sz = g.size;
    std::vector<unsigned char> pixels(sz * sz * 3);

    auto& grass = g.request_field<GrassField>();
    auto& tree = g.request_field<TreeField>();

    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            float H_soil = g.H_soil[y][x];
            float h_surf = g.h_surface[y][x];
            float grass_val = grass[y][x];
            float tree_val = tree[y][x];

            float h_L = x > 0 ? g.H_soil[y][x-1] : H_soil;
            float h_R = x < sz - 1 ? g.H_soil[y][x+1] : H_soil;
            float h_T = y > 0 ? g.H_soil[y-1][x] : H_soil;
            float h_B = y < sz - 1 ? g.H_soil[y+1][x] : H_soil;

            float dx = h_R - h_L;
            float dy = h_B - h_T;
            float shadow = 1.0f + 0.45f * dx + 0.45f * dy;
            float shadeFactor = std::max(0.35f, std::min(1.65f, shadow));

            bool isBeach = false;
            if (h_surf <= 0.02f) {
                for (int dy_b = -2; dy_b <= 2 && !isBeach; ++dy_b) {
                    for (int dx_b = -2; dx_b <= 2 && !isBeach; ++dx_b) {
                        int ny = y + dy_b;
                        int nx = x + dx_b;
                        if (ny >= 0 && ny < sz && nx >= 0 && nx < sz && g.h_surface[ny][nx] > 0.45f) {
                            isBeach = true;
                        }
                    }
                }
            }

            float r = 100.0f, g_val = 105.0f, b = 115.0f;
            if (isBeach) {
                r = 240.0f * shadeFactor;
                g_val = 218.0f * shadeFactor;
                b = 165.0f * shadeFactor;
            } else if (H_soil > -1.0f) {
                float height_t = std::min(1.0f, (H_soil + 1.0f) / 4.0f);
                r = ((1.0f - height_t) * 110.0f + height_t * 168.0f) * shadeFactor;
                g_val = ((1.0f - height_t) * 92.0f + height_t * 162.0f) * shadeFactor;
                b = ((1.0f - height_t) * 75.0f + height_t * 155.0f) * shadeFactor;
            } else {
                r = 45.0f * shadeFactor;
                g_val = 52.0f * shadeFactor;
                b = 68.0f * shadeFactor;
            }

            if (h_surf <= 0.002f) {
                if (grass_val > 0.05f) {
                    float t_g = std::min(0.85f, grass_val);
                    r = (1.0f - t_g) * r + t_g * 34.0f * shadeFactor;
                    g_val = (1.0f - t_g) * g_val + t_g * 197.0f * shadeFactor;
                    b = (1.0f - t_g) * b + t_g * 94.0f * shadeFactor;
                }
                if (tree_val > 0.05f) {
                    float t_t = std::min(0.90f, tree_val);
                    r = (1.0f - t_t) * r + t_t * 21.0f * shadeFactor;
                    g_val = (1.0f - t_t) * g_val + t_t * 128.0f * shadeFactor;
                    b = (1.0f - t_t) * b + t_t * 61.0f * shadeFactor;
                }
            }

            if (h_surf > 0.005f) {
                float depth = std::min(1.0f, h_surf / 1.5f);
                // Interpolate: shallow (depth = 0.0) -> dark purple (70, 20, 95), deep (depth = 1.0) -> dark blue (15, 25, 80)
                float wr = (1.0f - depth) * 70.0f + depth * 15.0f;
                float wg = (1.0f - depth) * 20.0f + depth * 25.0f;
                float wb = (1.0f - depth) * 95.0f + depth * 80.0f;
                float t_w = H_soil > -1.0f ? 0.95f : std::min(0.75f, 0.22f + h_surf / 4.0f);

                r = (1.0f - t_w) * r + t_w * wr * shadeFactor;
                g_val = (1.0f - t_w) * g_val + t_w * wg * shadeFactor;
                b = (1.0f - t_w) * b + t_w * wb * shadeFactor;
            }

            int idx_pixel = (y * sz + x) * 3;
            pixels[idx_pixel] = (unsigned char)std::max(0.0f, std::min(255.0f, r));
            pixels[idx_pixel + 1] = (unsigned char)std::max(0.0f, std::min(255.0f, g_val));
            pixels[idx_pixel + 2] = (unsigned char)std::max(0.0f, std::min(255.0f, b));
        }
    }

    stbi_write_png(filename.c_str(), sz, sz, 3, pixels.data(), sz * 3);
}

struct Vec3_3d {
    double x, y, z;
    Vec3_3d operator-(const Vec3_3d& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3_3d operator+(const Vec3_3d& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3_3d operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3_3d& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3_3d cross(const Vec3_3d& other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3_3d normalized() const {
        double len = length();
        return len > 0 ? Vec3_3d{x / len, y / len, z / len} : Vec3_3d{0, 0, 0};
    }
};

struct ColorRGBA_3d {
    unsigned char r, g, b, a;
};

inline void rasterize_triangle_3d(
    Vec3_3d p0, Vec3_3d p1, Vec3_3d p2, ColorRGBA_3d col,
    std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
    int width, int height) {

    int minX = (int)std::max(0.0, std::floor(std::min({p0.x, p1.x, p2.x})));
    int maxX = (int)std::min((double)width - 1, std::ceil(std::max({p0.x, p1.x, p2.x})));
    int minY = (int)std::max(0.0, std::floor(std::min({p0.y, p1.y, p2.y})));
    int maxY = (int)std::min((double)height - 1, std::ceil(std::max({p0.y, p1.y, p2.y})));

    auto edge_func = [](double ax, double ay, double bx, double by, double cx, double cy) {
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    };

    double area = edge_func(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
    if (std::abs(area) < 1e-6) return;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            double w0 = edge_func(p1.x, p1.y, p2.x, p2.y, x, y) / area;
            double w1 = edge_func(p2.x, p2.y, p0.x, p0.y, x, y) / area;
            double w2 = edge_func(p0.x, p0.y, p1.x, p1.y, x, y) / area;

            if (w0 >= -1e-5 && w1 >= -1e-5 && w2 >= -1e-5) {
                double depth = w0 * p0.z + w1 * p1.z + w2 * p2.z;
                int idx = y * width + x;
                
                if (depth >= z_buffer[idx]) {
                    z_buffer[idx] = depth;
                    int idx_pixel = idx * 4;
                    if (col.a == 255) {
                        pixels[idx_pixel] = col.r;
                        pixels[idx_pixel + 1] = col.g;
                        pixels[idx_pixel + 2] = col.b;
                        pixels[idx_pixel + 3] = 255;
                    } else {
                        double alpha = col.a / 255.0;
                        pixels[idx_pixel] = (unsigned char)(col.r * alpha + pixels[idx_pixel] * (1.0 - alpha));
                        pixels[idx_pixel + 1] = (unsigned char)(col.g * alpha + pixels[idx_pixel + 1] * (1.0 - alpha));
                        pixels[idx_pixel + 2] = (unsigned char)(col.b * alpha + pixels[idx_pixel + 2] * (1.0 - alpha));
                    }
                }
            }
        }
    }
}

void save_png_3d(const std::string& filename, Grid& g) {
    int sz = g.size;
    int width = 1000;
    int height = 650;
    
    // Background is dark gray matching CSS #030508 (r=3, g=5, b=8, a=255)
    std::vector<unsigned char> pixels(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4] = 3;
        pixels[i * 4 + 1] = 5;
        pixels[i * 4 + 2] = 8;
        pixels[i * 4 + 3] = 255;
    }
    std::vector<double> z_buffer(width * height, -1e18);

    auto& grass = g.request_field<GrassField>();
    auto& tree = g.request_field<TreeField>();

    double spacing = 5.2 * (128.0 / sz); // 1.3 for 512x512
    double heightScale = 38.0;

    // 1. Interpolate vertex heights & coordinates
    std::vector<std::vector<Vec3_3d>> V_proj(sz + 1, std::vector<Vec3_3d>(sz + 1));
    std::vector<std::vector<Vec3_3d>> V_proj_w(sz + 1, std::vector<Vec3_3d>(sz + 1));
    for (int y = 0; y <= sz; ++y) {
        for (int x = 0; x <= sz; ++x) {
            double sumSoil = 0.0;
            double sumWater = 0.0;
            int count = 0;
            for (int dy = -1; dy <= 0; ++dy) {
                for (int dx = -1; dx <= 0; ++dx) {
                    int cy = y + dy;
                    int cx = x + dx;
                    if (cy >= 0 && cy < sz && cx >= 0 && cx < sz) {
                        sumSoil += g.H_soil[cy][cx];
                        sumWater += g.h_surface[cy][cx];
                        count++;
                    }
                }
            }
            double z_soil = sumSoil / (count ? count : 1);
            double z_water = sumWater / (count ? count : 1);
            
            // Ground vertex
            double px = 500.0 + (x - y) * 0.866 * spacing;
            double py = 250.0 + (x + y) * 0.5 * spacing - z_soil * heightScale;
            // Back-to-front depth:
            double depth = (x + y) * 0.5 * spacing + z_soil * heightScale;
            V_proj[y][x] = { px, py, depth };

            // Water vertex
            double z_w_surf = z_soil + z_water;
            if (z_soil < 0.0) {
                z_w_surf = 0.0; // flat sea level surface
            }
            double py_w = 250.0 + (x + y) * 0.5 * spacing - z_w_surf * heightScale;
            V_proj_w[y][x] = { px, py_w, depth + 0.01 };
        }
    }

    // Directional light vector
    Vec3_3d light_dir = { 0.4082, 0.4082, 0.8164 }; // normalized {0.5, 0.5, 1.0}

    // 2. Traversal: Back-to-Front Painter's order
    for (int sum = 0; sum <= 2 * (sz - 1); ++sum) {
        for (int x = 0; x <= sum; ++x) {
            int y = sum - x;
            if (y >= 0 && y < sz && x < sz) {
                Vec3_3d pA = V_proj[y][x];
                Vec3_3d pB = V_proj[y][x + 1];
                Vec3_3d pC = V_proj[y + 1][x + 1];
                Vec3_3d pD = V_proj[y + 1][x];

                float H_val = g.H_soil[y][x];
                float h_w = g.h_surface[y][x];
                float grass_val = grass[y][x];
                float tree_val = tree[y][x];

                // Compute base cell color
                bool isBeach = false;
                if (h_w <= 0.02f) {
                    for (int dy_b = -2; dy_b <= 2 && !isBeach; ++dy_b) {
                        for (int dx_b = -2; dx_b <= 2 && !isBeach; ++dx_b) {
                            int ny = y + dy_b;
                            int nx = x + dx_b;
                            if (ny >= 0 && ny < sz && nx >= 0 && nx < sz && g.h_surface[ny][nx] > 0.45f) {
                                isBeach = true;
                            }
                        }
                    }
                }

                float r = 100.0f, g_val = 105.0f, b = 115.0f;
                if (isBeach) {
                    r = 240.0f; g_val = 218.0f; b = 165.0f;
                } else if (H_val > -1.0f) {
                    float height_t = std::min(1.0f, (H_val + 1.0f) / 4.0f);
                    r = (1.0f - height_t) * 110.0f + height_t * 168.0f;
                    g_val = (1.0f - height_t) * 92.0f + height_t * 162.0f;
                    b = (1.0f - height_t) * 75.0f + height_t * 155.0f;
                } else {
                    r = 45.0f; g_val = 52.0f; b = 68.0f;
                }

                if (h_w <= 0.002f) {
                    if (grass_val > 0.05f) {
                        float t_g = std::min(0.85f, grass_val);
                        r = (1.0f - t_g) * r + t_g * 34.0f;
                        g_val = (1.0f - t_g) * g_val + t_g * 197.0f;
                        b = (1.0f - t_g) * b + t_g * 94.0f;
                    }
                    if (tree_val > 0.05f) {
                        float t_t = std::min(0.90f, tree_val);
                        r = (1.0f - t_t) * r + t_t * 21.0f;
                        g_val = (1.0f - t_t) * g_val + t_t * 128.0f;
                        b = (1.0f - t_t) * b + t_t * 61.0f;
                    }
                }

                // Shading calculations for the two triangles
                Vec3_3d normal1 = (pB - pA).cross(pC - pA).normalized();
                double brightness1 = normal1.dot(light_dir) * 0.4 + 0.8;
                brightness1 = std::max(0.35, std::min(1.65, brightness1));
                ColorRGBA_3d col1 = {
                    (unsigned char)std::min(255.0, r * brightness1),
                    (unsigned char)std::min(255.0, g_val * brightness1),
                    (unsigned char)std::min(255.0, b * brightness1),
                    255
                };

                Vec3_3d normal2 = (pC - pA).cross(pD - pA).normalized();
                double brightness2 = normal2.dot(light_dir) * 0.4 + 0.8;
                brightness2 = std::max(0.35, std::min(1.65, brightness2));
                ColorRGBA_3d col2 = {
                    (unsigned char)std::min(255.0, r * brightness2),
                    (unsigned char)std::min(255.0, g_val * brightness2),
                    (unsigned char)std::min(255.0, b * brightness2),
                    255
                };

                // Draw ground triangles
                rasterize_triangle_3d(pA, pB, pC, col1, pixels, z_buffer, width, height);
                rasterize_triangle_3d(pA, pC, pD, col2, pixels, z_buffer, width, height);

                // Water overlay if active
                if (h_w > 0.005f) {
                    Vec3_3d pAw = V_proj_w[y][x];
                    Vec3_3d pBw = V_proj_w[y][x + 1];
                    Vec3_3d pCw = V_proj_w[y + 1][x + 1];
                    Vec3_3d pDw = V_proj_w[y + 1][x];

                    float depth = std::min(1.0f, h_w / 1.5f);
                    // Interpolate: shallow (depth = 0.0) -> dark purple (70, 20, 95), deep (depth = 1.0) -> dark blue (15, 25, 80)
                    float wr = (1.0f - depth) * 70.0f + depth * 15.0f;
                    float wg = (1.0f - depth) * 20.0f + depth * 25.0f;
                    float wb = (1.0f - depth) * 95.0f + depth * 80.0f;
                    
                    // Make it highly opaque (0.90 to 0.95 alpha) so we can see the dark colors clearly
                    unsigned char alpha = (unsigned char)(255.0f * (0.90f + 0.05f * depth));
                    
                    ColorRGBA_3d col_w = { (unsigned char)wr, (unsigned char)wg, (unsigned char)wb, alpha };

                    rasterize_triangle_3d(pAw, pBw, pCw, col_w, pixels, z_buffer, width, height);
                    rasterize_triangle_3d(pAw, pCw, pDw, col_w, pixels, z_buffer, width, height);
                }
            }
        }
    }

    stbi_write_png(filename.c_str(), width, height, 4, pixels.data(), width * 4);
}

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

void export_map_data(const std::string& filename, const std::vector<RealisticMapState>& history, float cell_spacing_m, float height_scale_m) {
    int grid_size = history.empty() ? 0 : history[0].H_soil.size();
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
    float base_mm_per_hr;
    float mountain_mm_per_hr;
public:
    RealisticPrecipitation(float base, float mountain, float dummy = 0.0f) 
        : base_mm_per_hr(base), mountain_mm_per_hr(mountain) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        // Convert mm/hour to meters/year: mm_to_m (1/1000) * hours_per_year (8760) = 8.76
        // Then convert meters/year to simulation vertical units/year: divide by height_scale_m
        float factor = 8.760f / g.scale.height_scale_m;
        float base_units_per_yr = base_mm_per_hr * factor;
        float mountain_units_per_yr = mountain_mm_per_hr * factor;

        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float H = g.H_soil[y][x];
                if (H > 0.0f) {
                    // Rain scales linearly with elevation to model smooth orographic rain
                    float scale = std::min(1.0f, H / 1.2f);
                    g.h_surface[y][x] += (base_units_per_yr + scale * mountain_units_per_yr) * dt;
                }
            }
        }
    }
};

#include <random>

void fill_depressions(Grid& g) {
    int sz = g.size;
    std::vector<std::vector<float>> F(sz, std::vector<float>(sz, 99999.0f));
    
    // Set boundaries to H_soil
    for (int x = 0; x < sz; ++x) {
        F[0][x] = g.H_soil[0][x];
        F[sz - 1][x] = g.H_soil[sz - 1][x];
    }
    for (int y = 0; y < sz; ++y) {
        F[y][0] = g.H_soil[y][0];
        F[y][sz - 1] = g.H_soil[y][sz - 1];
    }
    
    // Set ocean/coastal outlets
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            if (g.H_soil[y][x] <= 0.0f) {
                F[y][x] = g.H_soil[y][x];
            }
        }
    }
    
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 1000) {
        changed = false;
        
        // Forward scan
        for (int y = 1; y < sz - 1; ++y) {
            for (int x = 1; x < sz - 1; ++x) {
                if (F[y][x] <= g.H_soil[y][x]) continue;
                
                float min_f = 99999.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        min_f = std::min(min_f, F[y + dy][x + dx]);
                    }
                }
                
                float next_f = std::max(g.H_soil[y][x], min_f + 0.0001f);
                if (next_f < F[y][x]) {
                    F[y][x] = next_f;
                    changed = true;
                }
            }
        }
        
        // Backward scan
        for (int y = sz - 2; y >= 1; --y) {
            for (int x = sz - 2; x >= 1; --x) {
                if (F[y][x] <= g.H_soil[y][x]) continue;
                
                float min_f = 99999.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        min_f = std::min(min_f, F[y + dy][x + dx]);
                    }
                }
                
                float next_f = std::max(g.H_soil[y][x], min_f + 0.0001f);
                if (next_f < F[y][x]) {
                    F[y][x] = next_f;
                    changed = true;
                }
            }
        }
        iterations++;
    }
    
    // Write back filled topography
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            g.H_soil[y][x] = F[y][x];
        }
    }
    std::cout << "SUCCESS: Filled depressions in " << iterations << " iterations!" << std::endl;
}

void run_boulder_erosion(Grid& g, int num_boulders, float channel_depth_meters, float boulder_diameter_meters, std::vector<std::vector<bool>>& successful_channels) {
    float cell_w = g.scale.cell_spacing_m;
    float height_scale = g.scale.height_scale_m;
    
    float radius_m = boulder_diameter_meters / 2.0f;
    float radius_cells = radius_m / cell_w;
    float radius_units = radius_m / height_scale;
    float depth_units = channel_depth_meters / height_scale;
    
    int r_bound = std::ceil(radius_cells);
    
    // Copy the original uncarved topography to prevent compounding depth loops across different boulders
    std::vector<std::vector<float>> H_base(g.size, std::vector<float>(g.size));
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            H_base[y][x] = g.H_soil[y][x];
        }
    }
    
    // Stable seed for reproducible boulder drops
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, g.size - 1);
    
    int boulders_dropped = 0;
    int successful_carves = 0;
    while (boulders_dropped < num_boulders) {
        int x = 0;
        int y = 0;
        
        // Roulette-wheel selection weighted by altitude
        float total_weight = 0.0f;
        for (int j = 0; j < g.size; ++j) {
            for (int i = 0; i < g.size; ++i) {
                if (g.H_soil[j][i] > 0.0f) {
                    total_weight += g.H_soil[j][i];
                }
            }
        }
        
        if (total_weight <= 0.0f) break;
        
        std::uniform_real_distribution<float> f_dist(0.0f, total_weight);
        float target = f_dist(rng);
        float current_sum = 0.0f;
        bool found = false;
        for (int j = 0; j < g.size; ++j) {
            for (int i = 0; i < g.size; ++i) {
                if (g.H_soil[j][i] > 0.0f) {
                    current_sum += g.H_soil[j][i];
                    if (current_sum >= target) {
                        x = i;
                        y = j;
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
        
        boulders_dropped++;
        
        // 1. Trace the downhill path using a physics-based momentum and bridging model
        std::vector<std::pair<int, int>> path;
        int cur_x = x;
        int cur_y = y;
        bool hit_local_min = false;
        
        // Track visited cells to prevent perpetual motion loops/oscillations in pits
        std::vector<std::vector<bool>> visited(g.size, std::vector<bool>(g.size, false));
        visited[cur_y][cur_x] = true;
        
        // Initial specific kinetic energy (starting velocity = 5 m/s)
        float e_k = 0.5f * 5.0f * 5.0f; 
        
        while (true) {
            path.push_back({cur_x, cur_y});
            if (num_boulders <= 10) {
                std::cout << "[BOULDER DEBUG] Boulder " << boulders_dropped << " Step " << path.size() 
                          << ": cur=(" << cur_x << "," << cur_y << "), H=" << g.H_soil[cur_y][cur_x] * height_scale 
                          << "m, e_k=" << e_k << std::endl;
            }
            
            // Find steepest downhill neighbor among 8-neighbors to determine rolling direction
            int next_x = cur_x;
            int next_y = cur_y;
            float min_z = g.H_soil[cur_y][cur_x];
            
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int ny = cur_y + dy;
                    int nx = cur_x + dx;
                    if (ny >= 0 && ny < g.size && nx >= 0 && nx < g.size) {
                        float nz = g.H_soil[ny][nx];
                        if (nz < min_z) {
                            min_z = nz;
                            next_x = nx;
                            next_y = ny;
                        }
                    }
                }
            }
            
            // If the steepest descent neighbor is higher (or equal), search all neighbors for the lowest climb
            if (next_x == cur_x && next_y == cur_y) {
                float lowest_climb = 99999.0f;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int ny = cur_y + dy;
                        int nx = cur_x + dx;
                        if (ny >= 0 && ny < g.size && nx >= 0 && nx < g.size) {
                            float nz = g.H_soil[ny][nx];
                            if (nz < lowest_climb) {
                                lowest_climb = nz;
                                next_x = nx;
                                next_y = ny;
                            }
                        }
                    }
                }
            }
            
            // If still stuck (no neighbors available at all), stop
            if (next_x == cur_x && next_y == cur_y) {
                if (num_boulders <= 10) std::cout << "[BOULDER DEBUG] Stopped: No neighbors found." << std::endl;
                hit_local_min = true;
                break;
            }
            
            // Prevent backtracking loops
            if (visited[next_y][next_x]) {
                if (num_boulders <= 10) std::cout << "[BOULDER DEBUG] Stopped: Visited cell (" << next_x << "," << next_y << ") again." << std::endl;
                hit_local_min = true;
                break;
            }
            visited[next_y][next_x] = true;
            
            // Calculate energy transition to next cell
            float dz = (g.H_soil[next_y][next_x] - g.H_soil[cur_y][cur_x]) * height_scale;
            float d_cells = (next_x == cur_x || next_y == cur_y) ? cell_w : cell_w * 1.4142f;
            
            float de = 0.0f;
            float mu = 0.00f; // Rolling friction coefficient
            
            if (dz > 0.0f) {
                // Rolling uphill: calculate bridging clearance
                float w = d_cells;
                float R = boulder_diameter_meters / 2.0f;
                float delta = dz; // default if smaller than the hollow
                if (R * R > (w / 2.0f) * (w / 2.0f)) {
                    delta = R - std::sqrt(R * R - (w / 2.0f) * (w / 2.0f));
                }
                float h_barrier = std::min(dz, delta);
                de = -9.81f * h_barrier - mu * 9.81f * d_cells;
            } else {
                // Rolling downhill
                de = -9.81f * dz - mu * 9.81f * d_cells;
            }
            
            // If kinetic energy is depleted, the boulder is trapped
            if (e_k + de <= 0.0f) {
                if (num_boulders <= 10) std::cout << "[BOULDER DEBUG] Stopped: Kinetic energy depleted (e_k=" << e_k << ", de=" << de << ")." << std::endl;
                hit_local_min = true;
                break;
            }
            
            e_k += de;
            
            // Stop if it merges with a successful channel (including ocean/boundaries)
            if (successful_channels[next_y][next_x]) {
                path.push_back({next_x, next_y});
                break;
            }
            
            cur_x = next_x;
            cur_y = next_y;
        }
        
        // 2. Carve the cylindrical channel along the traced path (except the final local minimum)
        int carve_limit = hit_local_min ? (int)path.size() - 1 : (int)path.size();
        if (!hit_local_min) {
            successful_carves++;
            // Record path cells as successful channels
            for (const auto& pt : path) {
                successful_channels[pt.second][pt.first] = true;
            }
        }
        
        for (int i = 0; i < carve_limit; ++i) {
            int px = path[i].first;
            int py = path[i].second;
            
            // Calculate taper scaling factor s (ramp over 5 cells)
            float s = 1.0f;
            float ramp_cells = 5.0f;
            if (carve_limit > 1) {
                float s_start = (float)(i + 1) / ramp_cells;
                float s_end = (float)(carve_limit - i) / ramp_cells;
                s = std::max(0.0f, std::min(1.0f, std::min(s_start, s_end)));
            }
            
            float local_depth_units = depth_units * s;
            float local_radius_m = radius_m * s;
            int r_bound_local = std::ceil(local_radius_m / cell_w);
            
            if (local_radius_m < 0.1f) continue;
            
            float H_center = H_base[py][px]; // Read from static original topography
            
            for (int dy = -r_bound_local; dy <= r_bound_local; ++dy) {
                for (int dx = -r_bound_local; dx <= r_bound_local; ++dx) {
                    int ny = py + dy;
                    int nx = px + dx;
                    if (ny >= 0 && ny < g.size && nx >= 0 && nx < g.size) {
                        float d_cells = std::sqrt(dx * dx + dy * dy);
                        float d_meters = d_cells * cell_w;
                        if (d_meters <= local_radius_m) {
                            // Cosine funnel profile (bell-curve depression)
                            float d_factor = d_meters / local_radius_m;
                            float funnel_depth = local_depth_units * 0.5f * (1.0f + std::cos(3.14159265f * d_factor));
                            float z_cut = H_center - funnel_depth;
                            g.H_soil[ny][nx] = std::min(g.H_soil[ny][nx], z_cut);
                        }
                    }
                }
            }
        }
        
        // 3. If caught in a local minimum, deposit the boulder there (add a positive mound)
        if (hit_local_min && !path.empty()) {
            int px = path.back().first;
            int py = path.back().second;
            
            for (int dy = -r_bound; dy <= r_bound; ++dy) {
                for (int dx = -r_bound; dx <= r_bound; ++dx) {
                    int ny = py + dy;
                    int nx = px + dx;
                    if (ny >= 0 && ny < g.size && nx >= 0 && nx < g.size) {
                        float d_cells = std::sqrt(dx * dx + dy * dy);
                        float d_meters = d_cells * cell_w;
                        if (d_meters <= radius_m) {
                            // Cylindrical dome height profile for deposit:
                            float z_dome = depth_units * (1.0f - d_meters / radius_m);
                            g.H_soil[ny][nx] += z_dome;
                        }
                    }
                }
            }
        }
    }
    std::cout << "SUCCESS: Carved cylindrical paths for " << successful_carves << " / " << num_boulders << " boulders that reached the sea!" << std::endl;
}

int main() {
    std::system("mkdir -p flow/mountain_range_frames");
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

    // Define mountain range arc parameters (centered in the lower-left, curving across the island)
    float arc_cx = center_x - sz * 0.25f;
    float arc_cy = center_y - sz * 0.25f;
    float R_arc = sz * 0.45f;
    float angle_min = -0.3f; // Starts near horizontal
    float angle_max = 1.8f;  // Ends past vertical
    
    float ridge_w = sz * 0.08f; // Base width of the mountain range
    float max_h = 0.95f;        // Smaller max elevation of mountains (Gentler mountain range)

    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            // 1. Restore Circular Island Outline (No stretching)
            float dx = (float)x - center_x;
            float dy = (float)y - center_y;
            float r_dist = std::sqrt(dx*dx + dy*dy);

            float coast_u = 1.15f;
            float u_noise = perlin.noise(x * 0.035f, y * 0.035f) * 0.12f +
                            perlin.noise(x * 0.12f, y * 0.12f) * 0.04f;
            float u = r_dist / R_max + u_noise;

            if (u < coast_u) {
                // 2. Very flat plain base: only slopes from 0.08m to 0.02m (no steep doming)
                float land_t = u / coast_u;
                float z = 0.02f + 0.55f * std::cos(land_t * 1.5708f);

                // 3. Mountain Arc Profile: distance to closest point on arc segment
                float dx_arc = (float)x - arc_cx;
                float dy_arc = (float)y - arc_cy;
                float angle = std::atan2(dy_arc, dx_arc);
                
                // Clamp angle to the arc segment range
                float angle_clamped = std::max(angle_min, std::min(angle_max, angle));
                
                // Closest point on the arc
                float proj_x = arc_cx + R_arc * std::cos(angle_clamped);
                float proj_y = arc_cy + R_arc * std::sin(angle_clamped);
                
                float dist_to_ridge = std::sqrt(((float)x - proj_x)*((float)x - proj_x) + ((float)y - proj_y)*((float)y - proj_y));
                
                // Normalized position t along the arc from 0.0 to 1.0
                float t = (angle_clamped - angle_min) / (angle_max - angle_min);
                
                // Modulate peak height along the arc to create four individual peaks along the spine
                float peak_mod = 0.50f + 
                                 0.40f * std::exp(-((t - 0.15f) / 0.08f) * ((t - 0.15f) / 0.08f)) + 
                                 0.55f * std::exp(-((t - 0.38f) / 0.10f) * ((t - 0.38f) / 0.10f)) + 
                                 0.60f * std::exp(-((t - 0.62f) / 0.12f) * ((t - 0.62f) / 0.12f)) + 
                                 0.45f * std::exp(-((t - 0.85f) / 0.08f) * ((t - 0.85f) / 0.08f));
                
                float mountain_z = (max_h * peak_mod) / (1.0f + (dist_to_ridge / ridge_w) * (dist_to_ridge / ridge_w));
                
                // Add natural ridged details to the mountain to create ridges and V-shaped valleys
                float n_ridge = 0.70f * (1.0f - std::abs(perlin.noise(x * 0.025f, y * 0.025f))) +
                                0.30f * (1.0f - std::abs(perlin.noise(x * 0.07f, y * 0.07f)));
                
                // Seed V-shaped valleys across the entire landmass, scaled by mountain and plain slopes
                z += mountain_z * (0.25f + 1.15f * n_ridge) + 0.15f * n_ridge * (1.0f - land_t);

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

    // Configuration structure for history recording intervals (instrumentation stride)
    struct InstrumentationConfig {
        int genesis_stride = 1;     // Record every step of Genesis
        int noise_stride = 1;       // Record noise step
        int hydro_stride = 10;      // Record every 10 steps of Incision (12 frames total)
        int eco_stride = 100;       // Record every 100 steps of Eco-Vegetation (20 frames total)
    };

    InstrumentationConfig inst_config;
    if (const char* env_gen = std::getenv("GENESIS_STRIDE")) inst_config.genesis_stride = std::stoi(env_gen);
    if (const char* env_noise = std::getenv("NOISE_STRIDE")) inst_config.noise_stride = std::stoi(env_noise);
    if (const char* env_hyd = std::getenv("HYDRO_STRIDE")) inst_config.hydro_stride = std::stoi(env_hyd);
    if (const char* env_eco = std::getenv("ECO_STRIDE")) inst_config.eco_stride = std::stoi(env_eco);

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
    p2->add<RealisticPrecipitation>(0.0f, 0.742f, 0.20f); // 0.742 mm/hr (equivalent to 6.5 m/year) orographic rain
    p2->add<SubSteppedHydrodynamics>(9.81f, 0.018f, 60);
    // Physically, this models the easily erodible, soft alluvial sand/silt sediments of the lowlands plain (tau_c_physical ~0.1 Pa)
    // with a realistic critical shear stress threshold (0.02f) and sediment settling deposition (0.18f) to prevent shock base erosion.
    p2->add<Erosion>(0.45f, 0.02f, 1.62e-8f, 0.18f, 0.0f, 0.0f, 0.45f, false); 
 
    // 4. Phase 3: Coupled Eco-Vegetation
    const int steps_eco = 120;
    float dt_eco = 0.05f;
    Phase* p3 = orch.add_phase("Eco-Vegetation Growth", dt_eco, steps_eco, GRID_HIGH);
    p3->add<RealisticPrecipitation>(0.0f, 1.427f, 0.32f); // 1.427 mm/hr (equivalent to 12.5 m/year) orographic rain
    p3->add<SubSteppedHydrodynamics>(9.81f, 0.018f, 60);
    // Restore standard Erosion parameters with active sediment settling deposition (0.04f) to prevent channel clogging
    p3->add<Erosion>(0.15f, 0.04f, 1.62e-8f, 0.04f, 0.0035f, 0.015f, 0.45f, false);
    p3->add<Landslide>(0.55f, 511, 511); // High repose slope to stabilize river canyon walls
    p3->add<Vegetation>(110.0f, 25.0f, 1.0f); // Dynamic growth (grass & trees enabled)

    std::vector<RealisticMapState> history;
 
    // Helper to capture active simulation state at native resolution (no downsampling)
    auto record_history_state = [&](int global_step, const std::string& phase_name, bool force_record) {
        if (force_record) {
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

            // Save visual frame directly as PNG from C++
            std::string frame_filename = "flow/mountain_range_frames/frame_" + std::to_string(history.size() - 1) + ".png";
            save_png(frame_filename, g);
            std::string frame_filename_3d = "flow/mountain_range_frames/frame_3d_" + std::to_string(history.size() - 1) + ".png";
            save_png_3d(frame_filename_3d, g);
        }
    };
 
    record_history_state(0, "Landscape Genesis", true);

    // Run Phase 1
    std::cout << "Running Phase 1: Landscape Genesis..." << std::endl;
    orch.run_phase(p1, [&](int step) {
        bool record = ((step + 1) % inst_config.genesis_stride == 0) || (step == steps_tectonic - 1);
        record_history_state(step + 1, "Landscape Genesis", record);
    });
 
    // Run Boulder Erosion before adding micro-terrain noise and rain
    std::cout << "Running Phase: Depression/Sink Filling..." << std::endl;
    fill_depressions(g);
    
    // Declare and initialize the persistent successful channels map
    std::vector<std::vector<bool>> successful_channels(g.size, std::vector<bool>(g.size, false));
    for (int y = 0; y < g.size; ++y) {
        for (int x = 0; x < g.size; ++x) {
            if (g.H_soil[y][x] <= 0.0f ||
                x == 0 || x == g.size - 1 ||
                y == 0 || y == g.size - 1) {
                successful_channels[y][x] = true;
            }
        }
    }
    
    // Phase A: Carve major valleys with medium-large boulders
    std::cout << "Running Boulder Erosion Phase A: Major Valleys..." << std::endl;
    run_boulder_erosion(g, 100, 80.0f, 160.0f, successful_channels);
    
    // Phase B: Carve detailed tributaries with small boulders
    std::cout << "Running Boulder Erosion Phase B: Tributary Gullies..." << std::endl;
    run_boulder_erosion(g, 2000, 25.0f, 50.0f, successful_channels);
    
    record_history_state(steps_tectonic + 1, "Boulder Erosion", true);

    // Run Phase 1.5
    std::cout << "Running Phase: Micro-Terrain Noise..." << std::endl;
    orch.run_phase(p_noise, [&](int step) {
        bool record = ((step + 1) % inst_config.noise_stride == 0) || (step == steps_noise - 1);
        record_history_state(steps_tectonic + step + 2, "Micro-Terrain Noise", record);
    });

    // Run Phase 2
    std::cout << "Running Phase 2: Hydrology & River Incision..." << std::endl;
    orch.run_phase(p2, [&](int step) {
        // Maintain constant ocean level (z = 0.0) as Dirichlet boundary sink
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                if (g.H_soil[y][x] < 0.0f) {
                    g.h_surface[y][x] = 0.0f - g.H_soil[y][x];
                }
            }
        }
        bool record = ((step + 1) % inst_config.hydro_stride == 0) || (step == steps_hydro - 1);
        record_history_state(steps_tectonic + steps_noise + step + 2, "Hydrology & Incision", record);
    });
 
    // Run Phase 3
    std::cout << "Running Phase 3: Coupled Eco-Vegetation..." << std::endl;
    orch.run_phase(p3, [&](int step) {
        // Maintain constant ocean level (z = 0.0) as Dirichlet boundary sink
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                if (g.H_soil[y][x] < 0.0f) {
                    g.h_surface[y][x] = 0.0f - g.H_soil[y][x];
                }
            }
        }
        bool record = ((step + 1) % inst_config.eco_stride == 0) || (step == steps_eco - 1);
        record_history_state(steps_tectonic + steps_noise + steps_hydro + step + 1, "Eco-Vegetation Growth", record);
    });

    std::cout << "Exporting dataset to flow/test_mountain_range.js..." << std::endl;
    export_map_data("flow/test_mountain_range.js", history, g.scale.cell_spacing_m, g.scale.height_scale_m);
    std::cout << "SUCCESS: Exported successfully!" << std::endl;

    return 0;
}
