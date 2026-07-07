#ifndef EROSION_H
#define EROSION_H

#include "element.h"
#include <cmath>

// 3. Erosion, Deposition, and Transport Element
class Erosion : public Element {
private:
    float M_erosion;
    float tau_c;
    float C_friction;
    float settle_rate;
    float infiltration_rate;
    float K_sub;
    float max_soil_capacity;
    bool allow_bedrock_erosion;
public:
    Erosion(float M, float tc, float Cf, float sr, float ir, float k_sub = 0.08f, float cap = 0.40f, bool allow_bedrock = true)
        : M_erosion(M), tau_c(tc), C_friction(Cf), settle_rate(sr), infiltration_rate(ir), K_sub(k_sub), max_soil_capacity(cap), allow_bedrock_erosion(allow_bedrock) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;

        // 1. Infiltration capped by remaining soil water capacity
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float capacity_left = std::max(0.0f, max_soil_capacity - g.h_soil_water[y][x]);
                float potential_infilt = infiltration_rate * dt;
                float actual_infilt = std::min(potential_infilt, g.h_surface[y][x]);
                actual_infilt = std::min(actual_infilt, capacity_left);
                
                g.h_surface[y][x] -= actual_infilt;
                g.h_soil_water[y][x] += actual_infilt;
            }
        }

        // 2. Darcy Subsurface Flow (gravity-driven water table diffusion)
        std::vector<std::vector<float>> z_water(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                z_water[y][x] = g.H_soil[y][x] + g.h_soil_water[y][x];
            }
        }

        std::vector<std::vector<float>> next_soil_water = g.h_soil_water;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                        float head_diff = z_water[y][x] - z_water[ny][nx];
                        if (head_diff > 0.0f) {
                            float flow = K_sub * head_diff * (g.h_soil_water[y][x] / max_soil_capacity) * dt;
                            flow = std::min(flow, g.h_soil_water[y][x] * 0.23f);
                            next_soil_water[y][x] -= flow;
                            next_soil_water[ny][nx] += flow;
                        }
                    }
                }
            }
        }
        g.h_soil_water = next_soil_water;

        // 3. Return Flow / Seepage (Exfiltration of excess water to surface)
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (g.h_soil_water[y][x] > max_soil_capacity) {
                    float seepage = g.h_soil_water[y][x] - max_soil_capacity;
                    g.h_surface[y][x] += seepage;
                    g.h_soil_water[y][x] = max_soil_capacity;
                }
            }
        }

        // Diagnostic Water Progress Tracking
        static bool reached_mid = false;
        if (!reached_mid) {
            for (int y = 0; y < sz; ++y) {
                if (g.h_surface[y][sz / 2] > 0.01f) {
                    std::cout << ">>> WATER OVERFLOWED SADDLE (REACHED X=" << (sz / 2) << ") AT STEP " << step << " <<<" << std::endl;
                    reached_mid = true;
                    break;
                }
            }
        }
        
        static bool reached_end = false;
        if (!reached_end) {
            for (int y = 0; y < sz; ++y) {
                if (g.h_surface[y][sz - 5] > 0.01f) {
                    std::cout << ">>> WATER REACHED EAST EDGE (REACHED X=" << (sz - 5) << ") AT STEP " << step << " <<<" << std::endl;
                    reached_end = true;
                    break;
                }
            }
        }

        // Cache optional vegetation and soil pointers once per step
        const std::vector<std::vector<float>>* grass_ptr = g.has_field<GrassField>() ? &g.request_field<GrassField>() : nullptr;
        const std::vector<std::vector<float>>* tree_ptr = g.has_field<TreeField>() ? &g.request_field<TreeField>() : nullptr;
        std::vector<std::vector<float>>* soil_ptr = g.has_field<SoilField>() ? &g.request_field<SoilField>() : nullptr;

        // Coupled Bed Erosion
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (g.h_surface[y][x] > 0.01f) {
                    float vx_avg = (g.vx[x][y] + g.vx[x+1][y]) * 0.5f;
                    float vy_avg = (g.vy[x][y] + g.vy[x][y+1]) * 0.5f;
                    float u_mag = std::sqrt(vx_avg * vx_avg + vy_avg * vy_avg);
                    
                    float tau = C_friction * u_mag * u_mag;
                    
                    // Vegetation binding scales down the erodibility coefficient
                    float erodibility = M_erosion;
                    if (grass_ptr || tree_ptr) {
                        float grass_val = grass_ptr ? (*grass_ptr)[y][x] : 0.0f;
                        float tree_val = tree_ptr ? (*tree_ptr)[y][x] : 0.0f;
                        // Grass roots reduce water erosion by up to 90%, tree roots by up to 98%
                        erodibility *= std::exp(-2.3f * grass_val - 3.9f * tree_val);
                    }

                    if (tau > tau_c) {
                        float E = erodibility * (tau - tau_c) * dt;
                        if (!allow_bedrock_erosion && soil_ptr) {
                            E = std::min(E, (*soil_ptr)[y][x]);
                        }
                        E = std::min(E, g.H_soil[y][x] + 10.0f);
                        g.H_soil[y][x] -= E;
                        g.sediment[y][x] += E;
                        if (soil_ptr) {
                            (*soil_ptr)[y][x] = std::max(0.0f, (*soil_ptr)[y][x] - E);
                        }
                    } else {
                        float D = settle_rate * g.sediment[y][x] * dt;
                        g.sediment[y][x] -= D;
                        g.H_soil[y][x] += D;
                        if (soil_ptr) {
                            (*soil_ptr)[y][x] += D;
                        }
                    }
                }
            }
        }

        // Sediment Advection (Upwind transport)
        std::vector<std::vector<float>> next_sediment = g.sediment;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float vx_avg = (g.vx[x][y] + g.vx[x+1][y]) * 0.5f;
                float vy_avg = (g.vy[x][y] + g.vy[x][y+1]) * 0.5f;
                int nx = x + (vx_avg > 0.05f ? 1 : (vx_avg < -0.05f ? -1 : 0));
                int ny = y + (vy_avg > 0.05f ? 1 : (vy_avg < -0.05f ? -1 : 0));
                if (nx >= 0 && nx < sz && ny >= 0 && ny < sz && (nx != x || ny != y)) {
                    float amount = g.sediment[y][x] * 0.45f * dt;
                    next_sediment[y][x] -= amount;
                    next_sediment[ny][nx] += amount;
                }
            }
        }
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) g.sediment[y][x] = std::max(0.0f, next_sediment[y][x]);
        }

        // Conservative pairwise sediment smoothing
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for (int i = 0; i < 4; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                        float c_c = g.sediment[y][x] / std::max(0.01f, g.h_surface[y][x]);
                        float c_n = g.sediment[ny][nx] / std::max(0.01f, g.h_surface[ny][nx]);
                        float s_diff = c_c - c_n;
                        if (s_diff > 0.001f) {
                            float s_transfer = s_diff * 0.20f * dt * std::max(0.01f, g.h_surface[y][x]);
                            s_transfer = std::min(s_transfer, g.sediment[y][x] * 0.5f);
                            g.sediment[y][x] -= s_transfer;
                            g.sediment[ny][nx] += s_transfer;
                        }
                    }
                }
            }
        }
    }
};

#endif // EROSION_H
