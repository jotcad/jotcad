#ifndef HEX_TECTONIC_STRIKESLIP_H
#define HEX_TECTONIC_STRIKESLIP_H

#include "hex_element.h"
#include "perlin_noise.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Tectonic StrikeSlip Element on Hexagonal Grid representing horizontal shearing along a fault line segment
// with optional Perlin wobble and transpressional vertical deformation.
class HexTectonicStrikeSlip : public HexElement {
private:
    float q_a, r_a;
    float q_b, r_b;
    float total_slip_m; // Total slip in meters
    float k_inv_m;       // Width of shear zone (k parameter, higher = sharper)
    float wobble_amp_m; // Amplitude of fault line waviness in meters
    float C_uplift;      // Transpressional uplift/subsidence coefficient

public:
    HexTectonicStrikeSlip(float qa, float ra, float qb, float rb, float slip_m, float sharpness = 0.0005f, float wobble_m = 0.0f, float uplift = 0.0f)
        : q_a(qa), r_a(ra), q_b(qb), r_b(rb), total_slip_m(slip_m), k_inv_m(sharpness), wobble_amp_m(wobble_m), C_uplift(uplift) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        float step_slip = total_slip_m / (float)total_steps;
        float R_hex = g.scale.hex_radius_m;

        // Convert endpoints to Cartesian meters
        float x_a = R_hex * 1.7320508f * (q_a + r_a * 0.5f);
        float y_a = R_hex * 1.5f * r_a;
        float x_b = R_hex * 1.7320508f * (q_b + r_b * 0.5f);
        float y_b = R_hex * 1.5f * r_b;

        float dx = x_b - x_a;
        float dy = y_b - y_a;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;

        float vx_norm = dx / len;
        float vy_norm = dy / len;

        PerlinNoise2D perlin;
        
        // Helper to compute perturbed distance to fault line (in meters)
        auto get_dist_m = [&](float x, float y) {
            float dist = ((x_b - x_a) * (y - y_a) - (y_b - y_a) * (x - x_a)) / len;
            if (wobble_amp_m > 0.01f) {
                // Add organic Perlin wobble (scaled to physical meters)
                dist += wobble_amp_m * perlin.noise(x * 0.0001f, y * 0.0001f);
            }
            return dist;
        };

        // Compute displacement vectors (u_x, u_y) in meters
        int sq = g.size_q;
        int sr = g.size_r;
        std::vector<std::vector<float>> u_x(sr, std::vector<float>(sq, 0.0f));
        std::vector<std::vector<float>> u_y(sr, std::vector<float>(sq, 0.0f));
        std::vector<std::vector<float>> div(sr, std::vector<float>(sq, 0.0f));

        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                float x = R_hex * 1.7320508f * (q + r * 0.5f);
                float y = R_hex * 1.5f * r;

                float dist = get_dist_m(x, y);
                // Sigmoid shear factor
                float factor = std::tanh(k_inv_m * dist);
                u_x[r][q] = vx_norm * step_slip * factor;
                u_y[r][q] = vy_norm * step_slip * factor;
            }
        }

        // Calculate local divergence using central differences in Cartesian physical meters
        float cell_dx = R_hex * 1.7320508f;
        float cell_dy = R_hex * 1.5f;

        for (int r = 1; r < sr - 1; ++r) {
            for (int q = 1; q < sq - 1; ++q) {
                // Compute physical central derivatives: du/dx and dv/dy
                float du_dx = (u_x[r][q+1] - u_x[r][q-1]) / (2.0f * cell_dx);
                float du_dy = (u_y[r+1][q] - u_y[r-1][q]) / (2.0f * cell_dy);
                // Divergence (approximate strain)
                div[r][q] = du_dx + du_dy;
            }
        }

        // Apply transpressional vertical uplift/subsidence to bedrock and soil
        if (C_uplift > 0.001f) {
            for (int r = 1; r < sr - 1; ++r) {
                for (int q = 1; q < sq - 1; ++q) {
                    float val = C_uplift * div[r][q];
                    g.H_soil[r][q] -= val;
                    g.H_bedrock[r][q] -= val;
                }
            }
        }

        // Apply horizontal advection (bilinear lookup in axial q,r index space)
        auto old_soil = g.H_soil;
        auto old_bedrock = g.H_bedrock;
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                // Original Cartesian coordinates
                float x = R_hex * 1.7320508f * (q + r * 0.5f);
                float y = R_hex * 1.5f * r;

                // Source Cartesian coordinates
                float src_x = x - u_x[r][q];
                float src_y = y - u_y[r][q];

                // Convert back to fractional axial q,r coordinates
                float src_r = (2.0f * src_y) / (3.0f * R_hex);
                float src_q = src_x / (R_hex * 1.7320508f) - src_r * 0.5f;

                // Bilinear interpolation on index space (q, r)
                int q0 = std::floor(src_q);
                int r0 = std::floor(src_r);
                int q1 = q0 + 1;
                int r1 = r0 + 1;

                q0 = std::max(0, std::min(sq - 1, q0));
                q1 = std::max(0, std::min(sq - 1, q1));
                r0 = std::max(0, std::min(sr - 1, r0));
                r1 = std::max(0, std::min(sr - 1, r1));

                float tq = src_q - std::floor(src_q);
                float tr = src_r - std::floor(src_r);

                // Interpolate H_soil
                float hs00 = old_soil[r0][q0];
                float hs10 = old_soil[r0][q1];
                float hs01 = old_soil[r1][q0];
                float hs11 = old_soil[r1][q1];

                g.H_soil[r][q] = (1.0f - tq) * (1.0f - tr) * hs00 +
                                 tq * (1.0f - tr) * hs10 +
                                 (1.0f - tq) * tr * hs01 +
                                 tq * tr * hs11;

                // Interpolate H_bedrock
                float hb00 = old_bedrock[r0][q0];
                float hb10 = old_bedrock[r0][q1];
                float hb01 = old_bedrock[r1][q0];
                float hb11 = old_bedrock[r1][q1];

                g.H_bedrock[r][q] = (1.0f - tq) * (1.0f - tr) * hb00 +
                                    tq * (1.0f - tr) * hb10 +
                                    (1.0f - tq) * tr * hb01 +
                                    tq * tr * hb11;
            }
        }
    }
};

#endif // HEX_TECTONIC_STRIKESLIP_H
