#ifndef HEX_EXPORTER_H
#define HEX_EXPORTER_H

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "hex_grid.h"
#include "../fs/cpp/vendor/stb_image_write.h"

// State capture struct representing a single snapshot of the HexGrid simulation
struct HexGridState {
    int step;
    std::string phase_name;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> Q;
    std::vector<std::vector<float>> sediment;
    std::vector<std::vector<float>> vegetation;
    std::vector<std::vector<int>> downstream_dir;
    std::vector<std::vector<float>> h_lake;
};

// 3D vector helper for isometric projection & triangle rasterization
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

// Standard barycentric z-buffered triangle rasterizer
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
                    int idx_pixel = idx * 3;
                    if (col.a == 255) {
                        pixels[idx_pixel]     = col.r;
                        pixels[idx_pixel + 1] = col.g;
                        pixels[idx_pixel + 2] = col.b;
                    } else {
                        double alpha = col.a / 255.0;
                        pixels[idx_pixel]     = (unsigned char)(col.r * alpha + pixels[idx_pixel] * (1.0 - alpha));
                        pixels[idx_pixel + 1] = (unsigned char)(col.g * alpha + pixels[idx_pixel + 1] * (1.0 - alpha));
                        pixels[idx_pixel + 2] = (unsigned char)(col.b * alpha + pixels[idx_pixel + 2] * (1.0 - alpha));
                    }
                }
            }
        }
    }
}

// Helper to calculate identical, seam-free corner coordinates by averaging the centers of the 3 sharing cells
inline void get_vertex_coords(const HexGrid& g, int q, int r, int vertex_index, float cx, float cy, float R_px, float& vx, float& vy) {
    float sum_x = cx;
    float sum_y = cy;
    int count = 1;
    
    int dir1 = vertex_index;
    int dir2 = (vertex_index + 1) % 6;
    
    int nq1, nr1;
    if (g.get_neighbor(q, r, dir1, nq1, nr1)) {
        float nx1 = R_px * 1.7320508f * (nq1 + nr1 * 0.5f);
        float ny1 = R_px * 1.5f * nr1;
        sum_x += nx1;
        sum_y += ny1;
        count++;
    }
    int nq2, nr2;
    if (g.get_neighbor(q, r, dir2, nq2, nr2)) {
        float nx2 = R_px * 1.7320508f * (nq2 + nr2 * 0.5f);
        float ny2 = R_px * 1.5f * nr2;
        sum_x += nx2;
        sum_y += ny2;
        count++;
    }
    vx = sum_x / count;
    vy = sum_y / count;
}

// Helper to calculate smooth interpolated vertex height between a cell and its two adjacent neighbors
inline float get_vertex_height(const HexGrid& g, const std::vector<std::vector<float>>& h_lake, int q, int r, int vertex_index) {
    float H = g.H_soil[r][q] + g.h_surface[r][q] + h_lake[r][q];
    float sum_h = H;
    int count = 1;
    
    // Hex neighbors corresponding to adjacent directions for the vertex
    int dir1 = vertex_index;
    int dir2 = (vertex_index + 1) % 6;
    
    int nq1, nr1;
    if (g.get_neighbor(q, r, dir1, nq1, nr1)) {
        sum_h += g.H_soil[nr1][nq1] + g.h_surface[nr1][nq1] + h_lake[nr1][nq1];
        count++;
    }
    int nq2, nr2;
    if (g.get_neighbor(q, r, dir2, nq2, nr2)) {
        sum_h += g.H_soil[nr2][nq2] + g.h_surface[nr2][nq2] + h_lake[nr2][nq2];
        count++;
    }
    return sum_h / count;
}

// Projects coordinates (X, Y, Z) to isometric 2D screen coordinate (px, py, depth) using a South-facing 0-degree rotation
inline Vec3_3d project_iso(float X, float Y, float Z, float x_mid, float y_mid, float scale_xy, float scale_z) {
    double px = 500.0 + (X - x_mid) * scale_xy;
    double py = 320.0 + (Y - y_mid) * 0.5000000 * scale_xy - Z * scale_z;
    double depth = (Y - y_mid) * scale_xy + Z * scale_z;
    return { px, py, depth };
}

// Exports the collected simulation history to a high-performance binary file
inline void export_hex_to_binary(const std::string& filename, const std::vector<HexGridState>& history, int size_q, int size_r, float hex_radius, float dt) {
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Failed to open export file: " << filename << std::endl;
        return;
    }

    const char magic[4] = {'H', 'E', 'X', 'S'};
    out.write(magic, 4);

    int32_t q_size = size_q;
    int32_t r_size = size_r;
    int32_t steps_count = history.size();
    float radius = hex_radius;
    float step_dt = dt;

    out.write(reinterpret_cast<const char*>(&q_size), sizeof(q_size));
    out.write(reinterpret_cast<const char*>(&r_size), sizeof(r_size));
    out.write(reinterpret_cast<const char*>(&steps_count), sizeof(steps_count));
    out.write(reinterpret_cast<const char*>(&radius), sizeof(radius));
    out.write(reinterpret_cast<const char*>(&step_dt), sizeof(step_dt));

    int cell_count = q_size * r_size;
    std::vector<float> float_buffer(cell_count);
    std::vector<int32_t> int_buffer(cell_count);

    for (const auto& state : history) {
        int32_t step_idx = state.step;
        out.write(reinterpret_cast<const char*>(&step_idx), sizeof(step_idx));

        char name_buf[32];
        std::memset(name_buf, 0, 32);
        std::strncpy(name_buf, state.phase_name.c_str(), 31);
        out.write(name_buf, 32);

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.H_soil[r].begin(), state.H_soil[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.h_surface[r].begin(), state.h_surface[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.Q[r].begin(), state.Q[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.sediment[r].begin(), state.sediment[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.vegetation[r].begin(), state.vegetation[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.downstream_dir[r].begin(), state.downstream_dir[r].end(), int_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(int_buffer.data()), cell_count * sizeof(int32_t));

        for (int r = 0; r < r_size; ++r) {
            std::copy(state.h_lake[r].begin(), state.h_lake[r].end(), float_buffer.begin() + r * q_size);
        }
        out.write(reinterpret_cast<const char*>(float_buffer.data()), cell_count * sizeof(float));
    }

    out.close();
}

// Calculates the dynamic crop arability index [0.0, 1.0] for a cell based on soil, slope, and vegetation proxy
inline float calculate_cell_arability(const HexGrid& g, const std::vector<std::vector<float>>& h_lake, int q, int r) {
    float H = g.H_soil[r][q];
    float soil_thickness = H - g.H_bedrock[r][q];
    float water_depth = g.h_surface[r][q] + h_lake[r][q];

    if (water_depth > 0.30f) {
        return 0.0f; // Flooded land is not arable
    }

    // 1. Soil factor: deep soil is required
    float f_soil = 1.0f - std::exp(-soil_thickness / 0.40f);

    // 2. Slope factor: flat land is required
    float max_diff = 0.0f;
    for (int dir = 0; dir < 6; ++dir) {
        int nq, nr;
        if (g.get_neighbor(q, r, dir, nq, nr)) {
            float diff = std::abs(H - g.H_soil[nr][nq]);
            if (diff > max_diff) max_diff = diff;
        }
    }
    float slope = max_diff / 8660.0f;
    float f_slope = std::exp(-std::pow(slope / 0.08f, 2));

    // 3. Vegetation cover serves as the perfect proxy for moisture and temperature suitability
    float f_veg = g.vegetation[r][q];

    return f_soil * f_slope * f_veg;
}

// Bakes the HexGrid layout to a fully-shaded 2D PNG image directly in C++ using STB
inline void save_hex_png(const std::string& filename, const HexGrid& g, float R_px) {
    auto& h_lake = const_cast<HexGrid&>(g).request_field<HexLakeDepth>();
    float max_x = R_px * 1.7320508f * ((g.size_q - 1) + (g.size_r - 1) * 0.5f);
    float max_y = R_px * 1.5f * (g.size_r - 1);

    float margin_x = R_px * 1.7320508f * 1.5f;
    float margin_y = R_px * 1.5f * 1.5f;

    int w = std::ceil(max_x + 2.0f * margin_x);
    int h = std::ceil(max_y + 2.0f * margin_y);

    std::vector<unsigned char> pixels(w * h * 3, 0);

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

                // 1. Shading Calculation (ambient occlusion/hillshading)
                float h_E = g.is_valid(q + 1, r) ? g.H_soil[r][q + 1] : H;
                float h_W = g.is_valid(q - 1, r) ? g.H_soil[r][q - 1] : H;
                float h_N = g.is_valid(q, r - 1) ? g.H_soil[r - 1][q] : H;
                float h_S = g.is_valid(q, r + 1) ? g.H_soil[r + 1][q] : H;

                float dx_grad = h_E - h_W;
                float dy_grad = h_S - h_N;
                float shade = 1.0f; // Disabled hillshading for flat, vibrant coloring

                // 2. Eco-Hydrological Biome Coloring (Water-driven instead of altitude bands)
                float sr = 0.0f, sg = 0.0f, sb = 0.0f;

                float arability = calculate_cell_arability(g, h_lake, q, r);
                float alpha_arab = arability * 0.35f;

                if (H > 700.0f) {
                    // Mountain Peak (grey base, turning white/snow at the very top)
                    float snow_t = std::min(1.0f, (H - 700.0f) / 300.0f);
                    float rock_r = 110.0f * shade;
                    float rock_g = 105.0f * shade;
                    float rock_b = 100.0f * shade;
                    float snow_val = 245.0f * shade;

                    sr = (1.0f - snow_t) * rock_r + snow_t * snow_val;
                    sg = (1.0f - snow_t) * rock_g + snow_t * snow_val;
                    sb = (1.0f - snow_t) * rock_b + snow_t * snow_val;

                    // Apply soft arability overlay (does nothing on peaks where arability = 0)
                    sr = (1.0f - alpha_arab) * sr + alpha_arab * 235.0f;
                    sg = (1.0f - alpha_arab) * sg + alpha_arab * 45.0f;
                    sb = (1.0f - alpha_arab) * sb + alpha_arab * 45.0f;
                } else {
                    // Moisture factor: increases near river flows (Q) and standing water depth (water)
                    float Q_m3s = g.Q[r][q] / 31557600.0f;
                    float moisture_factor = std::min(1.0f, (Q_m3s / 30.0f) + (water / 0.5f));

                    // Grass: Bright yellowish-green [185, 230, 85]
                    // Forest: Bright forest green [20, 120, 45]
                    float target_r, target_g, target_b;
                    if (moisture_factor >= 0.35f) {
                        target_r = 20.0f;
                        target_g = 120.0f;
                        target_b = 45.0f;
                    } else {
                        target_r = 185.0f;
                        target_g = 230.0f;
                        target_b = 85.0f;
                    }

                    // Blend sand [195, 178, 130] or rock [135, 135, 135] based on soil thickness
                    float soil_t_m = H - g.H_bedrock[r][q];
                    float bedrock_blend = std::max(0.0f, std::min(1.0f, (0.50f - soil_t_m) / 0.50f));
                    
                    float substrate_r = (1.0f - bedrock_blend) * 195.0f + bedrock_blend * 135.0f;
                    float substrate_g = (1.0f - bedrock_blend) * 178.0f + bedrock_blend * 135.0f;
                    float substrate_b = (1.0f - bedrock_blend) * 130.0f + bedrock_blend * 135.0f;

                    // Apply soft arability overlay directly to the base substrate
                    float sub_r = (1.0f - alpha_arab) * substrate_r + alpha_arab * 235.0f;
                    float sub_g = (1.0f - alpha_arab) * substrate_g + alpha_arab * 45.0f;
                    float sub_b = (1.0f - alpha_arab) * substrate_b + alpha_arab * 45.0f;

                    // Blend vegetation on top of the arable substrate to preserve forest green vibrancy
                    sr = ((1.0f - veg) * sub_r + veg * target_r) * shade;
                    sg = ((1.0f - veg) * sub_g + veg * target_g) * shade;
                    sb = ((1.0f - veg) * sub_b + veg * target_b) * shade;
                }

                pr = (unsigned char)std::max(0.0f, std::min(255.0f, sr));
                pg = (unsigned char)std::max(0.0f, std::min(255.0f, sg));
                pb = (unsigned char)std::max(0.0f, std::min(255.0f, sb));

                // 3. Render water depth overlay directly on hex cells (Physical relief scaling)
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

                // 4. Boundary outline & topological contour lines
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

                    // Draw bright gold line for 150m contours
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
                            pr = 220;
                            pg = 150;
                            pb = 60;
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

    stbi_write_png(filename.c_str(), w, h, 3, pixels.data(), w * 3);
}

// Bakes the HexGrid layout to a 3D isometric projected PNG frame directly in C++
inline void save_hex_png_3d(const std::string& filename, const HexGrid& g, float R_px) {
    auto& h_lake = const_cast<HexGrid&>(g).request_field<HexLakeDepth>();
    int width = 1000;
    int height = 650;

    // Background matches the page dark green theme: #0d2419 (r=13, g=36, b=25)
    std::vector<unsigned char> pixels(width * height * 3);
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 3]     = 13;
        pixels[i * 3 + 1] = 36;
        pixels[i * 3 + 2] = 25;
    }

    std::vector<double> z_buffer(width * height, -1e18);

    // Compute center of grid in 2D axial layout
    float X_mid = R_px * 1.7320508f * (g.size_q - 1 + (g.size_r - 1) * 0.5f) * 0.5f;
    float Y_mid = R_px * 1.5f * (g.size_r - 1) * 0.5f;

    // Zoom/scale parameters to fit the 3D model on screen (scales dynamically with grid size)
    float scale_xy = 0.82f * (90.0f / g.size_q);
    float heightScale = 0.01f; // 1200m peak -> 12px vertical offset (30x vertical exaggeration)

    // Traversal: Back-to-Front Painter's Order
    for (int sum = 0; sum <= (g.size_q + g.size_r - 2); ++sum) {
        for (int r = 0; r <= sum; ++r) {
            int q = sum - r;
            if (q >= 0 && q < g.size_q && r >= 0 && r < g.size_r) {
                float H = g.H_soil[r][q];
                float water = g.h_surface[r][q] + h_lake[r][q];
                float veg = g.vegetation[r][q];

                float z_top = H + water;

                // 2D hex center
                float cx = R_px * 1.7320508f * (q + r * 0.5f);
                float cy = R_px * 1.5f * r;

                // 1. Shading Calculation (ambient occlusion/hillshading slope)
                float h_E = g.is_valid(q + 1, r) ? g.H_soil[r][q + 1] : H;
                float h_W = g.is_valid(q - 1, r) ? g.H_soil[r][q - 1] : H;
                float h_N = g.is_valid(q, r - 1) ? g.H_soil[r - 1][q] : H;
                float h_S = g.is_valid(q, r + 1) ? g.H_soil[r + 1][q] : H;

                float dx_grad = h_E - h_W;
                float dy_grad = h_S - h_N;
                float shade = 1.0f; // Disabled hillshading for flat, vibrant coloring

                // 2. Eco-Hydrological Biome Coloring (Water-driven instead of altitude bands)
                float sr = 0.0f, sg = 0.0f, sb = 0.0f;

                float arability = calculate_cell_arability(g, h_lake, q, r);
                float alpha_arab = arability * 0.35f;

                if (H > 700.0f) {
                    // Mountain Peak (grey base, turning white/snow at the very top)
                    float snow_t = std::min(1.0f, (H - 700.0f) / 300.0f);
                    float rock_r = 110.0f * shade;
                    float rock_g = 105.0f * shade;
                    float rock_b = 100.0f * shade;
                    float snow_val = 245.0f * shade;

                    sr = (1.0f - snow_t) * rock_r + snow_t * snow_val;
                    sg = (1.0f - snow_t) * rock_g + snow_t * snow_val;
                    sb = (1.0f - snow_t) * rock_b + snow_t * snow_val;

                    // Apply soft arability overlay (does nothing on peaks where arability = 0)
                    sr = (1.0f - alpha_arab) * sr + alpha_arab * 235.0f;
                    sg = (1.0f - alpha_arab) * sg + alpha_arab * 45.0f;
                    sb = (1.0f - alpha_arab) * sb + alpha_arab * 45.0f;
                } else {
                    // Moisture factor: increases near river flows (Q) and standing water depth (water)
                    float Q_m3s = g.Q[r][q] / 31557600.0f;
                    float moisture_factor = std::min(1.0f, (Q_m3s / 30.0f) + (water / 0.5f));

                    // Grass: Bright yellowish-green [185, 230, 85]
                    // Forest: Bright forest green [20, 120, 45]
                    float target_r, target_g, target_b;
                    if (moisture_factor >= 0.35f) {
                        target_r = 20.0f;
                        target_g = 120.0f;
                        target_b = 45.0f;
                    } else {
                        target_r = 185.0f;
                        target_g = 230.0f;
                        target_b = 85.0f;
                    }

                    // Blend sand [195, 178, 130] or rock [135, 135, 135] based on soil thickness
                    float soil_t_m = H - g.H_bedrock[r][q];
                    float bedrock_blend = std::max(0.0f, std::min(1.0f, (0.50f - soil_t_m) / 0.50f));
                    
                    float substrate_r = (1.0f - bedrock_blend) * 195.0f + bedrock_blend * 135.0f;
                    float substrate_g = (1.0f - bedrock_blend) * 178.0f + bedrock_blend * 135.0f;
                    float substrate_b = (1.0f - bedrock_blend) * 130.0f + bedrock_blend * 135.0f;

                    // Apply soft arability overlay directly to the base substrate
                    float sub_r = (1.0f - alpha_arab) * substrate_r + alpha_arab * 235.0f;
                    float sub_g = (1.0f - alpha_arab) * substrate_g + alpha_arab * 45.0f;
                    float sub_b = (1.0f - alpha_arab) * substrate_b + alpha_arab * 45.0f;

                    // Blend vegetation on top of the arable substrate to preserve forest green vibrancy
                    sr = ((1.0f - veg) * sub_r + veg * target_r) * shade;
                    sg = ((1.0f - veg) * sub_g + veg * target_g) * shade;
                    sb = ((1.0f - veg) * sub_b + veg * target_b) * shade;
                }

                // 3. Render water depth overlay (Physical relief scaling)
                if (water > 0.50f) {
                    float w_norm = std::min(1.0f, (water - 0.50f) / 1.00f);
                    float wr = 38.0f + (3.0f - 38.0f) * w_norm;
                    float wg = 145.0f + (80.0f - 145.0f) * w_norm;
                    float wb = 224.0f + (150.0f - 224.0f) * w_norm;

                    float opacity = w_norm * 0.92f;
                    sr = (1.0f - opacity) * sr + opacity * wr;
                    sg = (1.0f - opacity) * sg + opacity * wg;
                    sb = (1.0f - opacity) * sb + opacity * wb;
                }

                ColorRGBA_3d col_top = {
                    (unsigned char)std::max(0.0f, std::min(255.0f, sr)),
                    (unsigned char)std::max(0.0f, std::min(255.0f, sg)),
                    (unsigned char)std::max(0.0f, std::min(255.0f, sb)),
                    255
                };

                // Compute projected vertices (top and bottom base)
                Vec3_3d top_verts[6];
                Vec3_3d bot_verts[6];

                for (int i = 0; i < 6; ++i) {
                    float vx, vy;
                    get_vertex_coords(g, q, r, i, cx, cy, R_px, vx, vy);

                    // Smooth height interpolation at corners
                    float z_v = get_vertex_height(g, h_lake, q, r, i);

                    top_verts[i] = project_iso(vx, vy, z_v, X_mid, Y_mid, scale_xy, heightScale);
                    // Floating base thickness at Z = -100m to cover deep valleys
                    bot_verts[i] = project_iso(vx, vy, -100.0f, X_mid, Y_mid, scale_xy, heightScale);
                }

                Vec3_3d top_center = project_iso(cx, cy, z_top, X_mid, Y_mid, scale_xy, heightScale);

                // A. Rasterize Top Hexagonal Face (6 triangles)
                for (int i = 0; i < 6; ++i) {
                    rasterize_triangle_3d(top_center, top_verts[i], top_verts[(i + 1) % 6], col_top, pixels, z_buffer, width, height);
                }

                // B. Rasterize Front-Facing Side Walls (Only on the outer border of the grid)
                for (int i = 0; i <= 3; ++i) {
                    int nq, nr;
                    // Map wall index to the correct neighbor direction
                    int neighbor_dir = (i == 1) ? 5 : ((i == 2) ? 4 : i);

                    // Check if there is a neighbor in this direction
                    if (g.get_neighbor(q, r, neighbor_dir, nq, nr)) {
                        continue; // Do not draw walls between adjacent cells inside the grid!
                    }

                    float wall_shade = 1.0f;
                    if (i == 0) wall_shade = 0.90f; // East (semi-shaded)
                    if (i == 1) wall_shade = 0.82f; // South-East (shaded)
                    if (i == 2) wall_shade = 0.96f; // South-West (brightest front)
                    if (i == 3) wall_shade = 0.68f; // West (shadowed)

                    // Crust rocky color: [90, 80, 75]
                    ColorRGBA_3d col_wall = {
                        (unsigned char)std::min(255.0f, 90.0f * wall_shade),
                        (unsigned char)std::min(255.0f, 80.0f * wall_shade),
                        (unsigned char)std::min(255.0f, 75.0f * wall_shade),
                        255
                    };

                    // Wall side quad split into two triangles
                    rasterize_triangle_3d(top_verts[i], top_verts[i + 1], bot_verts[i + 1], col_wall, pixels, z_buffer, width, height);
                    rasterize_triangle_3d(top_verts[i], bot_verts[i + 1], bot_verts[i], col_wall, pixels, z_buffer, width, height);
                }
            }
        }
    }

    stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
}

#endif // HEX_EXPORTER_H
