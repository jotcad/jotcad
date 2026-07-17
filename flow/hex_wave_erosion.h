#ifndef HEX_WAVE_EROSION_H
#define HEX_WAVE_EROSION_H

#include "hex_element.h"
#include "hex_grid.h"
#include <algorithm>
#include <vector>

// Coastline Cellular Automata Wave Erosion & Deposition Model
class HexWaveErosion : public HexElement {
private:
    float erosion_rate; // K_wave_erosion (erosion coefficient)

public:
    HexWaveErosion(float rate = 0.000005f) : erosion_rate(rate) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        // Fetch fields
        if (!g.has_field<HexSeaBorder>()) return;
        auto& is_sea_border = g.request_field<HexSeaBorder>();

        // We will store height changes to apply them after the sweep to preserve cellular symmetry
        std::vector<std::vector<float>> dH(sr, std::vector<float>(sq, 0.0f));

        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                // Wave erosion only acts on land cells (H_soil >= 0.0) that are NOT sea borders
                if (g.H_soil[r][q] < 0.0f || is_sea_border[r][q] > 0.0f) continue;

                // Count ocean neighbors (neighbor cells that are below sea level or flagged as sea borders)
                int ocean_neighbors = 0;
                for (int dir = 0; dir < 6; ++dir) {
                    int nq, nr;
                    if (g.get_neighbor(q, r, dir, nq, nr)) {
                        if (g.H_soil[nr][nq] < 0.0f || is_sea_border[nr][nq] > 0.0f) {
                            ocean_neighbors++;
                        }
                    }
                }

                if (ocean_neighbors >= 1) {
                    // Soil erosion is 10x faster than bedrock erosion because it's loose sediment
                    float eroded_soil = erosion_rate * 10.0f * ocean_neighbors * dt;
                    float soil_thickness = std::max(0.0f, g.H_soil[r][q] - g.H_bedrock[r][q]);
                    float bedrock_erosion = 0.0f;
                    
                    if (ocean_neighbors >= 3) {
                        // High-energy headlands can erode bedrock if soil is fully stripped
                        if (eroded_soil > soil_thickness) {
                            float remaining_erosion = eroded_soil - soil_thickness;
                            eroded_soil = soil_thickness;
                            bedrock_erosion = remaining_erosion * 0.025f; // Bedrock is hard
                        }
                    } else {
                        // Straight shorelines and bays only erode soil, not bedrock
                        if (eroded_soil > soil_thickness) {
                            eroded_soil = soil_thickness;
                        }
                    }
                    
                    float total_erosion = eroded_soil + bedrock_erosion;
                    dH[r][q] -= total_erosion;

                    // Distribute 50% of the eroded sediment to adjacent sheltered land cells (longshore drift)
                    // The other 50% is washed offshore into the deep ocean (cross-shore transport)
                    if (total_erosion > 0.0f) {
                        std::vector<std::pair<int, int>> sheltered_neighbors;
                        for (int dir = 0; dir < 6; ++dir) {
                            int nq, nr;
                            if (g.get_neighbor(q, r, dir, nq, nr)) {
                                if (g.H_soil[nr][nq] >= 0.0f && is_sea_border[nr][nq] == 0.0f) {
                                    int n_ocean = 0;
                                    for (int nd = 0; nd < 6; ++nd) {
                                        int nnq, nnr;
                                        if (g.get_neighbor(nq, nr, nd, nnq, nnr)) {
                                            if (g.H_soil[nnr][nnq] < 0.0f || is_sea_border[nnr][nnq] > 0.0f) {
                                                n_ocean++;
                                            }
                                        }
                                    }
                                    if (n_ocean <= 2) {
                                        sheltered_neighbors.push_back({nq, nr});
                                    }
                                }
                            }
                        }

                        if (!sheltered_neighbors.empty()) {
                            float deposition_per_neighbor = (total_erosion * 0.5f) / sheltered_neighbors.size();
                            for (auto& neigh : sheltered_neighbors) {
                                dH[neigh.second][neigh.first] += deposition_per_neighbor;
                            }
                        }
                    }
                }
            }
        }

        // Apply height updates and propagate bedrock changes
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (dH[r][q] != 0.0f) {
                    float new_H = g.H_soil[r][q] + dH[r][q];
                    // If we eroded bedrock:
                    if (dH[r][q] < 0.0f) {
                        float soil_thick = g.H_soil[r][q] - g.H_bedrock[r][q];
                        float erosion_val = -dH[r][q];
                        if (erosion_val > soil_thick) {
                            g.H_bedrock[r][q] -= (erosion_val - soil_thick);
                        }
                    }
                    g.H_soil[r][q] = new_H;
                }
            }
        }
    }
};

#endif // HEX_WAVE_EROSION_H
