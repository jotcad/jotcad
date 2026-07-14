#ifndef HEX_LANDSLIDE_H
#define HEX_LANDSLIDE_H

#include "hex_element.h"
#include <algorithm>
#include <vector>
#include <cmath>

// Landslide / Slope Repose Element for HexGrid
class HexLandslide : public HexElement {
private:
    float max_slope;       // Maximum stable slope (m/m) (e.g. 0.15f corresponds to ~8.5 degrees)
    float veg_cohesion;    // How much vegetation increases slope stability (default ~0.5f for +50%)

public:
    HexLandslide(float slope = 0.15f, float veg_c = 0.5f)
        : max_slope(slope), veg_cohesion(veg_c) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        // Flat-to-flat distance between cells
        float dx = g.scale.hex_radius_m * 1.7320508f;

        // Run 3 sweeps of relaxation to allow soil slips to propagate naturally down steep valleys
        for (int sweep = 0; sweep < 3; ++sweep) {
            for (int r = 1; r < sr - 1; ++r) {
                for (int q = 1; q < sq - 1; ++q) {
                    float h_center = g.H_soil[r][q];

                    for (int dir = 0; dir < 6; ++dir) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, dir, nq, nr)) {
                            // Skip boundary edges
                            if (nq == 0 || nq == sq - 1 || nr == 0 || nr == sr - 1) continue;

                            float h_neighbor = g.H_soil[nr][nq];
                            float slope = (h_center - h_neighbor) / dx;

                            // Root cohesion increases local stable repose slope threshold
                            float local_max_slope = max_slope;
                            float veg_val = g.vegetation[r][q];
                            local_max_slope *= (1.0f + veg_cohesion * veg_val);

                            if (slope > local_max_slope) {
                                // Relax slope: move mass from center to neighbor
                                float excess = (slope - local_max_slope) * 0.15f;
                                float mass = excess * (dx * 0.5f); // mass-conserving slope relaxation

                                g.H_soil[r][q] -= mass;
                                g.H_soil[nr][nq] += mass;
                                h_center -= mass;
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif // HEX_LANDSLIDE_H
