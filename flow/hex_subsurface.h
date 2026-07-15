#ifndef HEX_SUBSURFACE_H
#define HEX_SUBSURFACE_H

#include "hex_element.h"
#include "hex_climate.h"
#include <algorithm>
#include <vector>
#include <cmath>

struct HexSubsurfaceParams {
    float infiltration_rate = 0.25f; // Annual fraction of surface water that infiltrates (1/yr)
    float conductivity = 120.0f;     // Saturated hydraulic conductivity K_sat (m/yr)
    int sub_steps = 15;              // Darcy sub-stepping iterations for numerical stability
};

class HexSubsurface : public HexElement {
private:
    HexSubsurfaceParams params;

public:
    HexSubsurface(const HexSubsurfaceParams& p = HexSubsurfaceParams()) : params(p) {}

    std::vector<std::type_index> get_required_fields() const override {
        return { std::type_index(typeid(HexGroundwater)) };
    }

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        auto& h_g = g.request_field<HexGroundwater>();
        int sq = g.size_q;
        int sr = g.size_r;
        float R = g.scale.hex_radius_m;

        // 1. Initialization on step 0
        if (step == 0) {
            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    float soil_thick = g.H_soil[r][q] - g.H_bedrock[r][q];
                    h_g[r][q] = 0.20f * std::max(0.0f, soil_thick);
                }
            }
        }

        // 2. Infiltration Phase: Surface water enters groundwater table
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                float h_surf = g.h_surface[r][q];
                if (h_surf > 0.0f) {
                    float max_g = g.H_soil[r][q] - g.H_bedrock[r][q];
                    float space = std::max(0.0f, max_g - h_g[r][q]);
                    float infil = std::min({h_surf, params.infiltration_rate * h_surf * dt, space});
                    g.h_surface[r][q] -= infil;
                    h_g[r][q] += infil;
                }
            }
        }

        // 3. Darcy Flow Sub-stepping: Lateral groundwater flow
        float sub_dt = dt / params.sub_steps;
        std::vector<std::vector<float>> H_wt(sr, std::vector<float>(sq, 0.0f));
        std::vector<std::vector<float>> next_h_g = h_g;
        std::vector<std::vector<float>> seepage_acc(sr, std::vector<float>(sq, 0.0f));

        for (int iter = 0; iter < params.sub_steps; ++iter) {
            // Update absolute water table heights (hydraulic head)
            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    H_wt[r][q] = g.H_bedrock[r][q] + next_h_g[r][q];
                }
            }

            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    float H_curr = H_wt[r][q];
                    float h_g_curr = next_h_g[r][q];
                    float max_g = g.H_soil[r][q] - g.H_bedrock[r][q];
                    
                    if (max_g <= 0.05f) {
                        next_h_g[r][q] = 0.0f;
                        continue;
                    }

                    float flow_sum = 0.0f;
                    for (int dir = 0; dir < 6; ++dir) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, dir, nq, nr)) {
                            float H_neigh = H_wt[nr][nq];
                            float h_g_neigh = next_h_g[nr][nq];
                            float max_g_neigh = g.H_soil[nr][nq] - g.H_bedrock[nr][nq];
                            
                            if (max_g_neigh <= 0.05f) continue;

                            float grad = H_neigh - H_curr;
                            float d_flow = 0.5f * (h_g_curr + h_g_neigh);

                            // Darcy flow transfer (m^3/yr)
                            float q_flow = (params.conductivity / 1.7320508f) * grad * d_flow;
                            flow_sum += q_flow;
                        }
                    }

                    // Compute depth change (m)
                    // Hex area = 2.598076 * R * R
                    float dh = (flow_sum / (2.598076f * R * R)) * sub_dt;
                    float new_h_g = h_g_curr + dh;

                    // If water table exceeds soil surface, accumulate seepage (spring exfiltration)
                    if (new_h_g > max_g) {
                        seepage_acc[r][q] += (new_h_g - max_g);
                        new_h_g = max_g;
                    }
                    next_h_g[r][q] = std::max(0.0f, new_h_g);
                }
            }
        }

        // Apply updated groundwater levels and exfiltrate springs to surface
        h_g = next_h_g;
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (seepage_acc[r][q] > 0.0f) {
                    g.h_surface[r][q] += seepage_acc[r][q];
                }
            }
        }
    }
};

#endif // HEX_SUBSURFACE_H
