#ifndef HEX_ROUTING_H
#define HEX_ROUTING_H

#include "hex_element.h"
#include <algorithm>
#include <vector>
#include <iostream>

class HexRouting : public HexElement {
private:
    float depth_coefficient; // Multiplier for hydraulic geometry (default ~0.15f)
    float depth_exponent;    // Exponent for hydraulic geometry (default ~0.4f)
    bool run_sink_fill;      // If true, fill depressions before routing

public:
    HexRouting(float coeff = 0.15f, float exponent = 0.4f, bool fill_sinks = true)
        : depth_coefficient(coeff), depth_exponent(exponent), run_sink_fill(fill_sinks) {}

    // Adapt Planchon-Darboux sink filling to the hexagonal grid
    void fill_depressions(HexGrid& g) {
        int sq = g.size_q;
        int sr = g.size_r;
        std::vector<std::vector<float>> F(sr, std::vector<float>(sq, 99999.0f));

        // 1. Initialize boundaries to H_soil
        for (int q = 0; q < sq; ++q) {
            F[0][q] = g.H_soil[0][q];
            F[sr - 1][q] = g.H_soil[sr - 1][q];
        }
        for (int r = 0; r < sr; ++r) {
            F[r][0] = g.H_soil[r][0];
            F[r][sq - 1] = g.H_soil[r][sq - 1];
        }

        // 2. Initialize ocean outlets to H_soil (sea level is 0.0)
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (g.H_soil[r][q] <= 0.0f) {
                    F[r][q] = g.H_soil[r][q];
                }
            }
        }

        // 3. Iterative forward-backward relaxation pass
        bool changed = true;
        int iterations = 0;
        while (changed && iterations < 1000) {
            changed = false;

            // Forward scan
            for (int r = 1; r < sr - 1; ++r) {
                for (int q = 1; q < sq - 1; ++q) {
                    if (F[r][q] <= g.H_soil[r][q]) continue;

                    float min_f = 99999.0f;
                    for (int dir = 0; dir < 6; ++dir) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, dir, nq, nr)) {
                            min_f = std::min(min_f, F[nr][nq]);
                        }
                    }

                    float next_f = std::max(g.H_soil[r][q], min_f + 0.0001f);
                    if (next_f < F[r][q]) {
                        F[r][q] = next_f;
                        changed = true;
                    }
                }
            }

            // Backward scan
            for (int r = sr - 2; r >= 1; --r) {
                for (int q = sq - 2; q >= 1; --q) {
                    if (F[r][q] <= g.H_soil[r][q]) continue;

                    float min_f = 99999.0f;
                    for (int dir = 0; dir < 6; ++dir) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, dir, nq, nr)) {
                            min_f = std::min(min_f, F[nr][nq]);
                        }
                    }

                    float next_f = std::max(g.H_soil[r][q], min_f + 0.0001f);
                    if (next_f < F[r][q]) {
                        F[r][q] = next_f;
                        changed = true;
                    }
                }
            }
            iterations++;
        }

        // 4. Write back filled elevations
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                g.H_soil[r][q] = F[r][q];
            }
        }
    }

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        // 1. Run depression filling if requested
        if (run_sink_fill) {
            fill_depressions(g);
        }

        // 2. Compute cell areas and water volume contributions
        // Area of pointy-topped hexagon = (3 * sqrt(3) / 2) * R^2
        float hex_area = 2.598076f * g.scale.hex_radius_m * g.scale.hex_radius_m;

        // 3. Initialize annual discharge with local precipitation volume
        // h_surface holds current local water depth (precip + previous surface water)
        std::vector<std::vector<float>> Q_accum(sr, std::vector<float>(sq, 0.0f));
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                // local annual rain volume: depth (m) * area (m^2)
                float local_vol_yr = g.h_surface[r][q] * hex_area;
                Q_accum[r][q] = local_vol_yr;
            }
        }

        // Reset downstream directions
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                g.downstream_dir[r][q] = -1;
            }
        }

        // 4. Create cell coordinate list and sort descending by elevation
        struct Cell {
            int q, r;
            float height;
        };
        std::vector<Cell> cells;
        cells.reserve(sq * sr);
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                cells.push_back({q, r, g.H_soil[r][q]});
            }
        }

        std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
            return a.height > b.height;
        });

        // 5. Propagate flow downhill (topological sweep)
        for (const auto& cell : cells) {
            int q = cell.q;
            int r = cell.r;
            float H_curr = g.H_soil[r][q];

            // Find steepest downhill neighbor
            int steepest_dir = -1;
            float max_slope = 0.0f;
            int steepest_nq = -1;
            int steepest_nr = -1;

            for (int dir = 0; dir < 6; ++dir) {
                int nq, nr;
                if (g.get_neighbor(q, r, dir, nq, nr)) {
                    float H_neigh = g.H_soil[nr][nq];
                    float drop = H_curr - H_neigh;
                    if (drop > max_slope) {
                        max_slope = drop;
                        steepest_dir = dir;
                        steepest_nq = nq;
                        steepest_nr = nr;
                    }
                }
            }

            // If a valid downhill neighbor exists, route all discharge to it
            if (steepest_dir != -1 && max_slope > 0.0f) {
                g.downstream_dir[r][q] = steepest_dir;
                Q_accum[steepest_nr][steepest_nq] += Q_accum[r][q];
            }
        }

        // 6. Write back Q (m3/yr) and calculate channel depth via hydraulic geometry
        // Seconds per year = 365.25 * 24 * 3600 = 31,557,600
        const float SECONDS_PER_YEAR = 31557600.0f;

        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                g.Q[r][q] = Q_accum[r][q];

                // Convert to m^3/s for hydraulic geometry calculations
                float Q_m3s = Q_accum[r][q] / SECONDS_PER_YEAR;

                if (Q_m3s > 0.0f) {
                    // h_surface (depth) calibrated to Q^exponent
                    g.h_surface[r][q] = depth_coefficient * std::pow(Q_m3s, depth_exponent);
                } else {
                    g.h_surface[r][q] = 0.0f;
                }
            }
        }
    }
};

#endif // HEX_ROUTING_H
