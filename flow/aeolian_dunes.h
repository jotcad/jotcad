#ifndef AEOLIAN_DUNES_H
#define AEOLIAN_DUNES_H

#include "element.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdlib>

// AeolianDunes Element representing wind-driven sand transport using a 2D deflected flow model
// and a physical wind-velocity advection solver (accelerating up slopes, decelerating behind crests and at toes)
class AeolianDunes : public Element {
private:
    float sand_input;      // Boundary sand load entering from the west
    float transport_coef;  // Base carrying capacity Q0
    float slope_influence; // Sensitivity to topography slope
    float max_slope;       // Sand repose slope angle (tan 32 degrees = 0.62)
    float relaxation;      // Rate of adaptation to equilibrium capacity
    std::vector<std::vector<float>> h_sand;

public:
    AeolianDunes(float input = 0.25f, float carry = 0.55f, float alpha = 12.0f, float repose = 0.62f, float relax = 1.50f)
        : sand_input(input), transport_coef(carry), slope_influence(alpha), max_slope(repose), relaxation(relax) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        if (h_sand.empty()) {
            // Seed random number generator with constant seed for deterministic test outputs
            std::srand(101);
            
            // Initialize with a thin sheet of sand plus column-correlated waves
            h_sand.assign(sz, std::vector<float>(sz, 0.18f));
            for (int x = 0; x < sz; ++x) {
                float wave = ((float)std::rand() / RAND_MAX - 0.5f) * 0.12f;
                for (int y = 0; y < sz; ++y) {
                    h_sand[y][x] = std::max(0.02f, h_sand[y][x] + wave);
                }
            }
        }

        // 1. Apply lateral (North-South) diffusion to weld sand patches into continuous ridges
        std::vector<std::vector<float>> smoothed = h_sand;
        for (int y = 1; y < sz - 1; ++y) {
            for (int x = 0; x < sz; ++x) {
                smoothed[y][x] = h_sand[y][x] * 0.84f + (h_sand[y-1][x] + h_sand[y+1][x]) * 0.08f;
            }
        }
        h_sand = smoothed;

        // Add minor continuous wind turbulence
        for (int x = 0; x < sz; ++x) {
            float wind_wave = ((float)std::rand() / RAND_MAX - 0.5f) * 0.005f;
            for (int y = 0; y < sz; ++y) {
                float cell_noise = ((float)std::rand() / RAND_MAX - 0.5f) * 0.002f;
                h_sand[y][x] = std::max(0.01f, h_sand[y][x] + wind_wave + cell_noise);
            }
        }

        std::vector<std::vector<float>> total_h(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                total_h[y][x] = g.H_soil[y][x] + h_sand[y][x];
            }
        }

        // 2. 2D Wind Field Deflection & Non-equilibrium Transport
        std::vector<std::vector<float>> q_s(sz, std::vector<float>(sz, sand_input));

        for (int y = 0; y < sz; ++y) {
            float U = 1.0f; // Wind velocity starting at baseline speed 1.0

            for (int x = 0; x < sz; ++x) {
                // Compute local slope and curvature in wind direction
                float slope = 0.0f;
                float curvature = 0.0f;
                if (x > 0) {
                    slope = total_h[y][x] - total_h[y][x-1];
                }
                if (x > 1) {
                    curvature = total_h[y][x] - 2.0f * total_h[y][x-1] + total_h[y][x-2];
                }

                // Update wind speed U:
                // - Accelerates windward uphill (slope > 0)
                // - Decelerates leeward downhill (slope < 0)
                // - Pressure blocking at the dune toe (positive curvature)
                float acc = 1.8f * std::max(0.0f, slope);
                float dec = 2.8f * std::max(0.0f, -slope);
                float block = 1.2f * std::max(0.0f, curvature);
                
                U = U + acc - dec - block;

                // Recovery relaxation toward baseline speed 1.0 over flat ground
                U = U + (1.0f - U) * 0.12f;
                U = std::max(0.15f, std::min(1.85f, U)); // Clamp wind speed to physical bounds

                // Bedrock gradient for wind deflection around obstacles
                float dh_dx = 0.0f;
                float dh_dy = 0.0f;
                if (x > 0 && x < sz - 1) dh_dx = (g.H_soil[y][x+1] - g.H_soil[y][x-1]) * 0.5f;
                if (y > 0 && y < sz - 1) dh_dy = (g.H_soil[y+1][x] - g.H_soil[y-1][x]) * 0.5f;

                // Wind vector: base (1.0, 0.0) deflected by bedrock obstacles
                float wx = 1.0f - 0.55f * std::max(0.0f, dh_dx);
                float wy = -0.55f * dh_dy * std::max(0.0f, dh_dx);

                // Upwind 2D sand load advection (Jacobi-style approximation)
                float q_west = x > 0 ? q_s[y][x-1] : sand_input;
                float q_actual = q_west;
                if (wy > 0.0f && y > 0) {
                    q_actual = (1.0f - wy) * q_west + wy * q_s[y-1][x];
                } else if (wy < 0.0f && y < sz - 1) {
                    q_actual = (1.0f + wy) * q_west - wy * q_s[y+1][x];
                }

                // Carrying capacity Q_c scales with wind velocity cubed (Bagnold's Law: Q ~ U^3)
                float Q_c = transport_coef * (U * U * U);

                // Sheet saturation limit (wind capacity drops when sand is thin)
                float saturation = std::min(1.0f, h_sand[y][x] / 0.40f);
                Q_c *= saturation;

                if (q_actual < Q_c) {
                    // Under-saturated wind: erode sand from bed
                    float erosion = relaxation * (Q_c - q_actual);
                    erosion = std::max(0.0f, std::min(h_sand[y][x], erosion));
                    h_sand[y][x] -= erosion * dt;
                    q_s[y][x] = q_actual + erosion;
                } else {
                    // Over-saturated wind: deposit sand onto bed
                    float deposition = relaxation * (q_actual - Q_c);
                    deposition = std::max(0.0f, std::min(q_actual, deposition));
                    h_sand[y][x] += deposition * dt;
                    q_s[y][x] = q_actual - deposition;
                }

                // Enforce bounds
                h_sand[y][x] = std::max(0.01f, h_sand[y][x]);
            }
        }

        // Outflow and lateral boundary conditions to prevent boundary edge pileup artifacts
        for (int y = 0; y < sz; ++y) {
            h_sand[y][sz-1] = h_sand[y][sz-2]; // East outflow
        }
        for (int x = 0; x < sz; ++x) {
            h_sand[0][x] = h_sand[1][x];       // North boundary
            h_sand[sz-1][x] = h_sand[sz-2][x]; // South boundary
        }

        // 3. Sand avalanching collapse (3 relaxation sweeps for numeric stability)
        for (int sweep = 0; sweep < 3; ++sweep) {
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    total_h[y][x] = g.H_soil[y][x] + h_sand[y][x];
                }
            }

            for (int y = 1; y < sz - 1; ++y) {
                for (int x = 1; x < sz - 1; ++x) {
                    if (h_sand[y][x] < 0.001f) continue;

                    int dx[] = {1, -1, 0, 0, 1, -1, 1, -1};
                    int dy[] = {0, 0, 1, -1, 1, 1, -1, -1};
                    float dist[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f};

                    for (int i = 0; i < 8; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        float slope = (total_h[y][x] - total_h[ny][nx]) / dist[i];
                        if (slope > max_slope) {
                            float excess = (slope - max_slope) * 0.15f;
                            float mass = std::min(excess * (dist[i] * 0.5f), h_sand[y][x]);
                            h_sand[y][x] -= mass;
                            h_sand[ny][nx] += mass;
                            total_h[y][x] -= mass;
                            total_h[ny][nx] += mass;
                        }
                    }
                }
            }
        }

        // Expose sand layer thickness to surface depth for unified visualization
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                g.h_surface[y][x] = h_sand[y][x];
            }
        }
    }
};

#endif // AEOLIAN_DUNES_H
