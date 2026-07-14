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

// Projects coordinates (X, Y, Z) to isometric 2D screen coordinate (px, py, depth)
inline Vec3_3d project_iso(float X, float Y, float Z, float x_mid, float y_mid, float scale_xy, float scale_z) {
    double px = 500.0 + (X - x_mid - (Y - y_mid)) * 0.8660254 * scale_xy;
    double py = 310.0 + (X - x_mid + (Y - y_mid)) * 0.5000000 * scale_xy - Z * scale_z;
    double depth = (X + Y) * 0.5000000 * scale_xy + Z * scale_z;
    return { px, py, depth };
}

// Exports the collected simulation history to a high-performance binary file
inline void export_hex_to_binary(const std::string& filename, const std::vector<HexGridState>& history, int size_q, int size_r, float hex_radius) {
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

    out.write(reinterpret_cast<const char*>(&q_size), sizeof(q_size));
    out.write(reinterpret_cast<const char*>(&r_size), sizeof(r_size));
    out.write(reinterpret_cast<const char*>(&steps_count), sizeof(steps_count));
    out.write(reinterpret_cast<const char*>(&radius), sizeof(radius));

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
    }

    out.close();
}

// Bakes the HexGrid layout to a fully-shaded 2D PNG image directly in C++ using STB
inline void save_hex_png(const std::string& filename, const HexGrid& g, float R_px) {
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
                float water = g.h_surface[r][q];
                float veg = g.vegetation[r][q];

                // 1. Shading Calculation (ambient occlusion/hillshading)
                float h_E = g.is_valid(q + 1, r) ? g.H_soil[r][q + 1] : H;
                float h_W = g.is_valid(q - 1, r) ? g.H_soil[r][q - 1] : H;
                float h_N = g.is_valid(q, r - 1) ? g.H_soil[r - 1][q] : H;
                float h_S = g.is_valid(q, r + 1) ? g.H_soil[r + 1][q] : H;

                float dx_grad = h_E - h_W;
                float dy_grad = h_S - h_N;
                float shadow = 1.0f - 0.002f * dx_grad - 0.002f * dy_grad;
                float shade = std::max(0.55f, std::min(1.45f, shadow));

                // 2. Continuous Biome Coloring (No sea-level clamping)
                float sr = 0.0f, sg = 0.0f, sb = 0.0f;

                if (H < 150.0f) {
                    // Lowlands / Grass plains / Sandy base
                    sr = (176.0f * (1.0f - veg) + veg * 46.0f) * shade;
                    sg = (158.0f * (1.0f - veg) + veg * 139.0f) * shade;
                    sb = (112.0f * (1.0f - veg) + veg * 52.0f) * shade;
                } else if (H < 600.0f) {
                    // Highland forest / rocky hills
                    sr = (130.0f * (1.0f - veg) + veg * 30.0f) * shade;
                    sg = (115.0f * (1.0f - veg) + veg * 95.0f) * shade;
                    sb = (85.0f * (1.0f - veg) + veg * 40.0f) * shade;
                } else {
                    // Mountain Peak (grey base, turning white/snow at the very top)
                    float snow_t = std::min(1.0f, (H - 600.0f) / 400.0f);
                    float rock_r = 110.0f * shade;
                    float rock_g = 105.0f * shade;
                    float rock_b = 100.0f * shade;
                    float snow_val = 245.0f * shade;

                    sr = (1.0f - snow_t) * rock_r + snow_t * snow_val;
                    sg = (1.0f - snow_t) * rock_g + snow_t * snow_val;
                    sb = (1.0f - snow_t) * rock_b + snow_t * snow_val;
                }

                pr = (unsigned char)std::max(0.0f, std::min(255.0f, sr));
                pg = (unsigned char)std::max(0.0f, std::min(255.0f, sg));
                pb = (unsigned char)std::max(0.0f, std::min(255.0f, sb));

                // 3. Render water depth overlay directly on hex cells (Physical relief scaling)
                if (water > 0.05f) {
                    float w_norm = std::min(1.0f, water / 1.50f);
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
    float heightScale = 0.11f; // 1200m peak -> 132px vertical offset

    // Traversal: Back-to-Front Painter's Order
    for (int sum = 0; sum <= (g.size_q + g.size_r - 2); ++sum) {
        for (int r = 0; r <= sum; ++r) {
            int q = sum - r;
            if (q >= 0 && q < g.size_q && r >= 0 && r < g.size_r) {
                float H = g.H_soil[r][q];
                float water = g.h_surface[r][q];
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
                float shadow = 1.0f - 0.002f * dx_grad - 0.002f * dy_grad;
                float shade = std::max(0.55f, std::min(1.45f, shadow));

                // 2. Continuous Biome Coloring (No sea-level clamping)
                float sr = 0.0f, sg = 0.0f, sb = 0.0f;

                if (H < 150.0f) {
                    sr = (176.0f * (1.0f - veg) + veg * 46.0f) * shade;
                    sg = (158.0f * (1.0f - veg) + veg * 139.0f) * shade;
                    sb = (112.0f * (1.0f - veg) + veg * 52.0f) * shade;
                } else if (H < 600.0f) {
                    sr = (130.0f * (1.0f - veg) + veg * 30.0f) * shade;
                    sg = (115.0f * (1.0f - veg) + veg * 95.0f) * shade;
                    sb = (85.0f * (1.0f - veg) + veg * 40.0f) * shade;
                } else {
                    float snow_t = std::min(1.0f, (H - 600.0f) / 400.0f);
                    float rock_r = 110.0f * shade;
                    float rock_g = 105.0f * shade;
                    float rock_b = 100.0f * shade;
                    float snow_val = 245.0f * shade;

                    sr = (1.0f - snow_t) * rock_r + snow_t * snow_val;
                    sg = (1.0f - snow_t) * rock_g + snow_t * snow_val;
                    sb = (1.0f - snow_t) * rock_b + snow_t * snow_val;
                }

                // 3. Render water depth overlay (Physical relief scaling)
                if (water > 0.05f) {
                    float w_norm = std::min(1.0f, water / 1.50f);
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
                    float angle_rad = (60.0f * i - 30.0f) * 3.14159265f / 180.0f;
                    float vx = cx + R_px * std::cos(angle_rad);
                    float vy = cy + R_px * std::sin(angle_rad);

                    top_verts[i] = project_iso(vx, vy, z_top, X_mid, Y_mid, scale_xy, heightScale);
                    // Floating base thickness at Z = -100m to cover deep valleys
                    bot_verts[i] = project_iso(vx, vy, -100.0f, X_mid, Y_mid, scale_xy, heightScale);
                }

                Vec3_3d top_center = project_iso(cx, cy, z_top, X_mid, Y_mid, scale_xy, heightScale);

                // A. Rasterize Top Hexagonal Face (6 triangles)
                for (int i = 0; i < 6; ++i) {
                    rasterize_triangle_3d(top_center, top_verts[i], top_verts[(i + 1) % 6], col_top, pixels, z_buffer, width, height);
                }

                // B. Rasterize Front-Facing Side Walls (South-East, South-West, West)
                for (int i = 1; i <= 3; ++i) {
                    float wall_shade = 1.0f;
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
