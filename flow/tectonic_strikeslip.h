#ifndef TECTONIC_STRIKESLIP_H
#define TECTONIC_STRIKESLIP_H

#include "element.h"
#include "perlin_noise.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Tectonic StrikeSlip Element representing horizontal shearing along a fault line segment
// with optional Perlin wobble and transpressional vertical deformation
class TectonicStrikeSlip : public Element {
private:
    float x_a, y_a;
    float x_b, y_b;
    float total_slip;
    float k;          // Sharpness of shear zone
    float wobble_amp; // Amplitude of fault line waviness
    float C_uplift;   // Transpressional uplift/subsidence coefficient

public:
    TectonicStrikeSlip(float xa, float ya, float xb, float yb, float slip, float sharpness = 0.5f, float wobble = 0.0f, float uplift = 0.0f)
        : x_a(xa), y_a(ya), x_b(xb), y_b(yb), total_slip(slip), k(sharpness), wobble_amp(wobble), C_uplift(uplift) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        float step_slip = total_slip / (float)total_steps;

        float dx = x_b - x_a;
        float dy = y_b - y_a;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;

        float vx_norm = dx / len;
        float vy_norm = dy / len;

        PerlinNoise2D perlin;
        
        // Helper to compute perturbed distance to fault line
        auto get_dist = [&](float x, float y) {
            float dist = ((x_b - x_a) * (y - y_a) - (y_b - y_a) * (x - x_a)) / len;
            if (wobble_amp > 0.01f) {
                // Add organic Perlin wobble to the fault trace
                dist += wobble_amp * perlin.noise(x * 0.08f, y * 0.08f);
            }
            return dist;
        };

        // Compute displacement vectors (u_x, u_y) and local divergence
        std::vector<std::vector<float>> u_x(sz, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> u_y(sz, std::vector<float>(sz, 0.0f));
        std::vector<std::vector<float>> div(sz, std::vector<float>(sz, 0.0f));

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float dist = get_dist(x, y);
                float factor = std::tanh(k * dist);
                u_x[y][x] = vx_norm * step_slip * factor;
                u_y[y][x] = vy_norm * step_slip * factor;
            }
        }

        // Calculate local divergence using central differences
        for (int y = 1; y < sz - 1; ++y) {
            for (int x = 1; x < sz - 1; ++x) {
                float du_dx = (u_x[y][x+1] - u_x[y][x-1]) * 0.5f;
                float du_dy = (u_y[y+1][x] - u_y[y-1][x]) * 0.5f;
                div[y][x] = du_dx + du_dy;
            }
        }

        // Apply transpressional vertical uplift/subsidence
        if (C_uplift > 0.001f) {
            for (int y = 1; y < sz - 1; ++y) {
                for (int x = 1; x < sz - 1; ++x) {
                    // Compression (div < 0) raises soil; Extension (div > 0) lowers soil
                    g.H_soil[y][x] -= C_uplift * div[y][x];
                }
            }
        }

        // Apply horizontal advection (bilinear lookup)
        auto old_soil = g.H_soil;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float src_x = (float)x - u_x[y][x];
                float src_y = (float)y - u_y[y][x];

                int x0 = std::floor(src_x);
                int y0 = std::floor(src_y);
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                x0 = std::max(0, std::min(sz - 1, x0));
                x1 = std::max(0, std::min(sz - 1, x1));
                y0 = std::max(0, std::min(sz - 1, y0));
                y1 = std::max(0, std::min(sz - 1, y1));

                float tx = src_x - std::floor(src_x);
                float ty = src_y - std::floor(src_y);

                float h00 = old_soil[y0][x0];
                float h10 = old_soil[y0][x1];
                float h01 = old_soil[y1][x0];
                float h11 = old_soil[y1][x1];

                g.H_soil[y][x] = (1.0f - tx) * (1.0f - ty) * h00 +
                                 tx * (1.0f - ty) * h10 +
                                 (1.0f - tx) * ty * h01 +
                                 tx * ty * h11;
            }
        }
    }
};

#endif // TECTONIC_STRIKESLIP_H
