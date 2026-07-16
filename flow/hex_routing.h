#ifndef HEX_ROUTING_H
#define HEX_ROUTING_H

#include "hex_element.h"
#include <algorithm>
#include <vector>
#include <iostream>

struct HexRoutingParams {
    float depth_coefficient = 0.15f;
    float depth_exponent = 0.4f;
    bool run_sink_fill = true;
    float evap_coefficient = 0.0f; // Default: no evaporation
};

class HexRouting : public HexElement {
private:
    float depth_coefficient; // Multiplier for hydraulic geometry (default ~0.15f)
    float depth_exponent;    // Exponent for hydraulic geometry (default ~0.4f)
    bool run_sink_fill;      // If true, fill depressions before routing

    // Climate parameters for lake evaporation
    float base_temp;
    float lapse_rate_divisor;
    float evap_coefficient;

public:
    HexRouting(float coeff = 0.15f, float exponent = 0.4f, bool fill_sinks = true)
        : depth_coefficient(coeff), depth_exponent(exponent), run_sink_fill(fill_sinks),
          base_temp(25.0f), lapse_rate_divisor(85.0f), evap_coefficient(0.0f) {}

    HexRouting(const ClimateProfile& profile, const HexRoutingParams& params)
        : depth_coefficient(params.depth_coefficient),
          depth_exponent(params.depth_exponent),
          run_sink_fill(params.run_sink_fill),
          base_temp(profile.base_temp),
          lapse_rate_divisor(profile.lapse_rate_divisor),
          evap_coefficient(params.evap_coefficient) {}

    // Adapt Planchon-Darboux sink filling to the hexagonal grid
    void fill_depressions(HexGrid& g) {
        int sq = g.size_q;
        int sr = g.size_r;
        float dx = g.scale.hex_radius_m * 1.7320508f;
        float outward_slope = 0.015f; // 1.5% slope
        float drop = outward_slope * dx;

        // Enforce constant outward slope on H_soil for all border cells
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (r == 0 || r == sr - 1 || q == 0 || q == sq - 1) {
                    float min_interior_h = 99999.0f;
                    for (int dir = 0; dir < 6; ++dir) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, dir, nq, nr)) {
                            if (nr > 0 && nr < sr - 1 && nq > 0 && nq < sq - 1) {
                                min_interior_h = std::min(min_interior_h, g.H_soil[nr][nq]);
                            }
                        }
                    }
                    if (min_interior_h < 99999.0f) {
                        g.H_soil[r][q] = min_interior_h - drop;
                    }
                }
            }
        }

        std::vector<std::vector<float>> F(sr, std::vector<float>(sq, 99999.0f));

        // 1. Initialize boundaries to H_soil, but lock sea borders to at least sea level (0.0m)
        std::vector<std::vector<float>>* is_sea_border = nullptr;
        if (g.has_field<HexSeaBorder>()) {
            is_sea_border = &g.request_field<HexSeaBorder>();
        }

        for (int q = 0; q < sq; ++q) {
            F[0][q] = (is_sea_border && (*is_sea_border)[0][q] > 0.0f) ? std::max(0.0f, g.H_soil[0][q]) : g.H_soil[0][q];
            F[sr - 1][q] = (is_sea_border && (*is_sea_border)[sr - 1][q] > 0.0f) ? std::max(0.0f, g.H_soil[sr - 1][q]) : g.H_soil[sr - 1][q];
        }
        for (int r = 0; r < sr; ++r) {
            F[r][0] = (is_sea_border && (*is_sea_border)[r][0] > 0.0f) ? std::max(0.0f, g.H_soil[r][0]) : g.H_soil[r][0];
            F[r][sq - 1] = (is_sea_border && (*is_sea_border)[r][sq - 1] > 0.0f) ? std::max(0.0f, g.H_soil[r][sq - 1]) : g.H_soil[r][sq - 1];
        }

        // 2. Iterative forward-backward relaxation pass
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

        // 4. Calculate lake depth (filled level - soil level) instead of overwriting H_soil
        auto& h_lake = g.request_field<HexLakeDepth>();
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                h_lake[r][q] = std::max(0.0f, F[r][q] - g.H_soil[r][q]);
            }
        }
    }

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        // 1. Run depression filling to find maximum brim capacities
        if (run_sink_fill) {
            fill_depressions(g);
        }

                auto& h_lake = g.request_field<HexLakeDepth>();
        float hex_area = 2.598076f * g.scale.hex_radius_m * g.scale.hex_radius_m;

        std::vector<std::vector<float>>* is_sea_border = nullptr;
        if (g.has_field<HexSeaBorder>()) {
            is_sea_border = &g.request_field<HexSeaBorder>();
        }

        // Keep a copy of brim capacities
        std::vector<std::vector<float>> h_lake_brim(sr, std::vector<float>(sq, 0.0f));
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                h_lake_brim[r][q] = h_lake[r][q];
            }
        }

        // Initialize Q_accum with local precipitation volume
        std::vector<std::vector<float>> Q_accum(sr, std::vector<float>(sq, 0.0f));
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                Q_accum[r][q] = g.h_surface[r][q] * hex_area;
            }
        }

        // Reset downstream directions
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                g.downstream_dir[r][q] = -1;
            }
        }

        // Create cell coordinate list and sort descending by brim-filled surface elevation
        struct Cell {
            int q, r;
            float water_surface_height;
        };
        std::vector<Cell> cells;
        cells.reserve(sq * sr);
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                cells.push_back({q, r, g.H_soil[r][q] + h_lake_brim[r][q]});
            }
        }
        std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
            return a.water_surface_height > b.water_surface_height;
        });

        // Pass 1: Global Routing to accumulate inflows
        for (const auto& cell : cells) {
            int q = cell.q;
            int r = cell.r;
            float F_curr = cell.water_surface_height;

            // Find steepest downhill neighbor on the filled surface
            int steepest_dir = -1;
            float max_slope = 0.0f;
            int steepest_nq = -1;
            int steepest_nr = -1;

            for (int dir = 0; dir < 6; dir++) {
                int nq, nr;
                if (g.get_neighbor(q, r, dir, nq, nr)) {
                    float F_neigh = g.H_soil[nr][nq] + h_lake_brim[nr][nq];
                    float drop = F_curr - F_neigh;
                    if (drop > max_slope) {
                        max_slope = drop;
                        steepest_dir = dir;
                        steepest_nq = nq;
                        steepest_nr = nr;
                    }
                }
            }

            float surplus = Q_accum[r][q];
            // Subtract local PET only for cells outside of any basin (h_lake_brim == 0)
            if (evap_coefficient > 0.0f && h_lake_brim[r][q] == 0.0f) {
                float temp = base_temp - (g.H_soil[r][q] / lapse_rate_divisor);
                float PET = evap_coefficient * std::max(0.0f, temp);
                float pet_vol = PET * hex_area * dt;
                surplus = std::max(0.0f, Q_accum[r][q] - pet_vol);
            }

            if (steepest_dir != -1 && max_slope > 0.0f && surplus > 0.0f) {
                g.downstream_dir[r][q] = steepest_dir;
                Q_accum[steepest_nr][steepest_nq] += surplus;
            }
            Q_accum[r][q] = surplus;
        }

        // 2. Identify contiguous lake basins (connected components where h_lake_brim > 0)
        std::vector<std::vector<int>> basin_id(sr, std::vector<int>(sq, -1));
        int next_basin_id = 0;
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (h_lake_brim[r][q] > 0.0f && basin_id[r][q] == -1) {
                    std::vector<std::pair<int, int>> queue;
                    queue.push_back({q, r});
                    basin_id[r][q] = next_basin_id;
                    int head = 0;
                    while (head < queue.size()) {
                        auto curr = queue[head++];
                        int cq = curr.first;
                        int cr = curr.second;
                        for (int dir = 0; dir < 6; ++dir) {
                            int nq, nr;
                            if (g.get_neighbor(cq, cr, dir, nq, nr)) {
                                if (h_lake_brim[nr][nq] > 0.0f && basin_id[nr][nq] == -1) {
                                    basin_id[nr][nq] = next_basin_id;
                                    queue.push_back({nq, nr});
                                }
                             }
                        }
                    }
                    next_basin_id++;
                }
            }
        }

        // Group cells by basin component
        std::vector<std::vector<std::pair<int, int>>> basins(next_basin_id);
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if (basin_id[r][q] != -1) {
                    basins[basin_id[r][q]].push_back({q, r});
                }
            }
        }

        // Structure to track overflows to route during Pass 2
        struct BasinOverflow {
            int spill_nq = -1;
            int spill_nr = -1;
            float surplus_vol = 0.0f;
            int outlet_q = -1;
            int outlet_r = -1;
        };
        std::vector<BasinOverflow> overflows(next_basin_id);

        // 3. Solve water balance for each basin
        for (int i = 0; i < next_basin_id; ++i) {
            // Sort cells in basin ascending by H_soil
            std::sort(basins[i].begin(), basins[i].end(), [&g](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                return g.H_soil[a.second][a.first] < g.H_soil[b.second][b.first];
            });

            // Find outlet/spillway of the basin
            int outlet_q = basins[i][0].first;
            int outlet_r = basins[i][0].second;
            int spill_nq = -1, spill_nr = -1;
            int spill_dir = -1;
            bool has_spillway = false;
            float min_F = 99999.0f;

            for (auto cell : basins[i]) {
                int q = cell.first;
                int r = cell.second;
                int dir = g.downstream_dir[r][q];
                if (dir >= 0 && dir < 6) {
                    int nq = q + HEX_DQ[dir];
                    int nr = r + HEX_DR[dir];
                    if (g.is_valid(nq, nr) && basin_id[nr][nq] != i) {
                        float F_val = g.H_soil[r][q] + h_lake_brim[r][q];
                        if (F_val < min_F) {
                            min_F = F_val;
                            outlet_q = q;
                            outlet_r = r;
                            spill_nq = nq;
                            spill_nr = nr;
                            spill_dir = dir;
                            has_spillway = true;
                        }
                    }
                }
            }

            // Inflow volume at the outlet of Pass 1 routing (represents total basin inflow)
            float total_inflow_vol = Q_accum[outlet_r][outlet_q];

            // Sum potential evaporation of the entire basin
            float total_pet_vol = 0.0f;
            for (auto cell : basins[i]) {
                int q = cell.first;
                int r = cell.second;
                float temp = base_temp - (g.H_soil[r][q] / lapse_rate_divisor);
                float PET = evap_coefficient * std::max(0.0f, temp);
                total_pet_vol += PET * hex_area * dt;
            }

            // Check if this basin contains any sea border cells
            bool is_ocean_basin = false;
            if (is_sea_border) {
                for (auto cell : basins[i]) {
                    if ((*is_sea_border)[cell.second][cell.first] > 0.0f) {
                        is_ocean_basin = true;
                        break;
                    }
                }
            }

            if ((has_spillway && total_inflow_vol >= total_pet_vol) || is_ocean_basin) {
                // Basin overflows or is the ocean! Keep brim capacities and outlet direction
                for (auto cell : basins[i]) {
                    int q = cell.first;
                    int r = cell.second;
                    h_lake[r][q] = h_lake_brim[r][q];
                }
                if (is_ocean_basin) {
                    overflows[i] = {-1, -1, 0.0f, -1, -1};
                } else {
                    overflows[i] = {spill_nq, spill_nr, total_inflow_vol - total_pet_vol, outlet_q, outlet_r};
                }
            } else {
                // Basin does not overflow (or is closed). Find flat water level H_water.
                float H_water = g.H_soil[basins[i][0].second][basins[i][0].first];
                if (total_inflow_vol > 0.0f && total_pet_vol > 0.0f) {
                    float cum_pet = 0.0f;
                    int j = 0;
                    for (; j < basins[i].size(); ++j) {
                        int q = basins[i][j].first;
                        int r = basins[i][j].second;
                        float temp = base_temp - (g.H_soil[r][q] / lapse_rate_divisor);
                        float PET = evap_coefficient * std::max(0.0f, temp);
                        float pet_val = PET * hex_area * dt;
                        if (cum_pet + pet_val >= total_inflow_vol) {
                            float fraction = (total_inflow_vol - cum_pet) / pet_val;
                            float H_prev = (j == 0) ? g.H_soil[basins[i][0].second][basins[i][0].first] : g.H_soil[basins[i][j-1].second][basins[i][j-1].first];
                            float H_curr = g.H_soil[r][q];
                            H_water = H_prev + fraction * (H_curr - H_prev);
                            break;
                        }
                        cum_pet += pet_val;
                    }
                    if (j == basins[i].size()) {
                        H_water = g.H_soil[basins[i].back().second][basins[i].back().first] + h_lake_brim[basins[i].back().second][basins[i].back().first];
                    }
                }

                // Set flat lake depths
                for (auto cell : basins[i]) {
                    int q = cell.first;
                    int r = cell.second;
                    h_lake[r][q] = std::max(0.0f, H_water - g.H_soil[r][q]);
                }

                // Redirect downstream directions of non-overflowing basin cells downhill on H_soil
                for (auto cell : basins[i]) {
                    int q = cell.first;
                    int r = cell.second;
                    int best_dir = -1;
                    float max_drop = 0.0f;
                    for (int d = 0; d < 6; ++d) {
                        int dnq, dnr;
                        if (g.get_neighbor(q, r, d, dnq, dnr)) {
                            float drop = g.H_soil[r][q] - g.H_soil[dnr][dnq];
                            if (drop > max_drop) {
                                max_drop = drop;
                                best_dir = d;
                            }
                        }
                    }
                    g.downstream_dir[r][q] = best_dir;
                }
            }
        }

        // Pass 2: Final routing using actual water level sorting and corrected directions
        std::vector<Cell> final_cells;
        final_cells.reserve(sq * sr);
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                final_cells.push_back({q, r, g.H_soil[r][q] + h_lake[r][q]});
            }
        }
        std::sort(final_cells.begin(), final_cells.end(), [](const Cell& a, const Cell& b) {
            return a.water_surface_height > b.water_surface_height;
        });

        // Re-initialize Q_accum with local precipitation volume
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                Q_accum[r][q] = g.h_surface[r][q] * hex_area;
            }
        }

        for (const auto& cell : final_cells) {
            int q = cell.q;
            int r = cell.r;

            // Check if this is the outlet of an overflowing basin
            int b_id = basin_id[r][q];
            bool is_spillway = false;
            float overflow_vol = 0.0f;
            int spill_nq = -1, spill_nr = -1;

            if (b_id != -1 && overflows[b_id].outlet_q == q && overflows[b_id].outlet_r == r) {
                is_spillway = true;
                overflow_vol = overflows[b_id].surplus_vol;
                spill_nq = overflows[b_id].spill_nq;
                spill_nr = overflows[b_id].spill_nr;
            }

            float surplus = Q_accum[r][q];
            if (is_spillway) {
                surplus = overflow_vol;
                if (spill_nq != -1) {
                    Q_accum[spill_nr][spill_nq] += surplus;
                }
            } else {
                // Apply local PET transmission loss only outside basins
                if (evap_coefficient > 0.0f && h_lake[r][q] == 0.0f) {
                    float temp = base_temp - (g.H_soil[r][q] / lapse_rate_divisor);
                    float PET = evap_coefficient * std::max(0.0f, temp);
                    float pet_vol = PET * hex_area * dt;
                    surplus = std::max(0.0f, Q_accum[r][q] - pet_vol);
                }

                int dir = g.downstream_dir[r][q];
                if (dir >= 0 && dir < 6) {
                    int nq = q + HEX_DQ[dir];
                    int nr = r + HEX_DR[dir];
                    Q_accum[nr][nq] += surplus;
                }
            }
            Q_accum[r][q] = surplus;
        }

        // Calculate channel depths from Pass 2 discharge values
        const float SECONDS_PER_YEAR = 31557600.0f;
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                g.Q[r][q] = Q_accum[r][q] / dt;
                float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;
                if (Q_m3s > 0.0f) {
                    g.h_surface[r][q] = depth_coefficient * std::pow(Q_m3s, depth_exponent);
                } else {
                    g.h_surface[r][q] = 0.0f;
                }
            }
        }

        // 5. Update soil salinity: Diffuse from sea borders and dilute by river discharge
        auto& salinity = g.request_field<HexSalinity>();
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                if ((is_sea_border && (*is_sea_border)[r][q] > 0.0f) || g.H_soil[r][q] < 0.0f) {
                    salinity[r][q] = 1.0f;
                } else {
                    if (step == 0) {
                        salinity[r][q] = 0.0f;
                    }
                }
            }
        }

        // 3 Jacobi iterations of diffusion and dilution
        for (int iter = 0; iter < 3; ++iter) {
            std::vector<std::vector<float>> next_sal(sr, std::vector<float>(sq, 0.0f));
            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    if ((is_sea_border && (*is_sea_border)[r][q] > 0.0f) || g.H_soil[r][q] < 0.0f) {
                        next_sal[r][q] = 1.0f;
                        continue;
                    }

                    float sum = 0.0f;
                    int count = 0;
                    for (int d = 0; d < 6; ++d) {
                        int nq, nr;
                        if (g.get_neighbor(q, r, d, nq, nr)) {
                            sum += salinity[nr][nq];
                            count++;
                        }
                    }

                    float avg_sal = count > 0 ? sum / count : 0.0f;
                    float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;
                    float dilution = std::min(1.0f, Q_m3s * 0.5f); // 2 m3/s completely flushes salinity
                    next_sal[r][q] = avg_sal * (1.0f - dilution);
                }
            }
            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    salinity[r][q] = next_sal[r][q];
                }
            }
        }
    }
};

#endif // HEX_ROUTING_H
