#ifndef HEX_EROSION_H
#define HEX_EROSION_H

#include "hex_element.h"
#include <algorithm>
#include <vector>
#include <cmath>

// Coupled Stream Power Erosion & Sediment Transport/Deposition Solver on HexGrid
class HexErosion : public HexElement {
private:
    float erodibility;        // Erosion rate coefficient (K_e)
    float capacity_coeff;     // Sediment capacity coefficient (K_c)
    float deposition_rate;    // Fraction of capacity surplus deposited per step
    float veg_shielding;      // Exponential shielding exponent by vegetation (lambda)

public:
    HexErosion(float k_e = 1.0e-5f, float k_c = 0.05f, float k_d = 0.1f, float lambda = 2.0f)
        : erodibility(k_e), capacity_coeff(k_c), deposition_rate(k_d), veg_shielding(lambda) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        // Flat-to-flat cell distance: R * sqrt(3)
        float dx = g.scale.hex_radius_m * 1.7320508f;
        // Pointy-topped hexagon area
        float hex_area = 2.598076f * g.scale.hex_radius_m * g.scale.hex_radius_m;
        // Soil density (~2000 kg/m^3) to convert volume to mass
        const float SOIL_DENSITY = 2000.0f;
        const float SECONDS_PER_YEAR = 31557600.0f;

        // Initialize sediment load (kg/yr) for each cell
        std::vector<std::vector<float>> sediment_load(sr, std::vector<float>(sq, 0.0f));

        auto& h_lake = g.request_field<HexLakeDepth>();

        // Create topological sorting list (highest first) based on water surface elevation
        struct Cell {
            int q, r;
            float water_surface_height;
        };
        std::vector<Cell> cells;
        cells.reserve(sq * sr);
        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                cells.push_back({q, r, g.H_soil[r][q] + h_lake[r][q]});
            }
        }

        std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
            return a.water_surface_height > b.water_surface_height;
        });

        // Run sediment routing & stream-power erosion
        for (const auto& cell : cells) {
            int q = cell.q;
            int r = cell.r;
            float F_curr = cell.water_surface_height;

            // 1. Calculate local slope (S) to downstream receiver along the water surface
            int next_dir = g.downstream_dir[r][q];
            float slope = 0.0f;
            int nq = -1, nr = -1;

            if (next_dir != -1 && g.get_neighbor(q, r, next_dir, nq, nr)) {
                float F_neigh = g.H_soil[nr][nq] + h_lake[nr][nq];
                slope = std::max(0.0f, (F_curr - F_neigh) / dx);
            } else if (r == 0 || r == sr - 1 || q == 0 || q == sq - 1) {
                // Border cell: match the slope of the incoming rivers to prevent delta damming
                float max_incoming_slope = 0.015f; // Base slope
                for (int dir = 0; dir < 6; ++dir) {
                    int nq_in, nr_in;
                    if (g.get_neighbor(q, r, dir, nq_in, nr_in)) {
                        // Check if the neighbor drains into this border cell
                        if (g.downstream_dir[nr_in][nq_in] == (dir + 3) % 6) {
                            float F_neigh = g.H_soil[nr_in][nq_in] + h_lake[nr_in][nq_in];
                            float incoming_slope = (F_neigh - F_curr) / dx;
                            max_incoming_slope = std::max(max_incoming_slope, incoming_slope);
                        }
                    }
                }
                slope = max_incoming_slope;
            }

            // 2. Convert cumulative discharge to m^3/s for physical law scaling
            float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;

            // 3. Compute sediment transport capacity (kg over step dt): Cap = K_c * Q * S * dt
            // (Units: m^3/s * slope * density * scale * dt_years)
            float capacity = capacity_coeff * Q_m3s * slope * SOIL_DENSITY * SECONDS_PER_YEAR * dt;

            // 4. Calculate vegetation binding factor (exponential reduction of erodibility)
            float veg = g.vegetation[r][q];
            float k_eff = erodibility * std::exp(-veg_shielding * veg);

            float current_load = sediment_load[r][q];

            if (current_load > capacity) {
                // Deposition: overloaded river deposits excess sediment
                float excess = current_load - capacity;
                // Fraction deposited over time-step dt (continuous exponential decay model)
                float dep_fraction = 1.0f - std::exp(-deposition_rate * dt);
                float deposited_mass = excess * dep_fraction;
                float deposited_height = deposited_mass / (hex_area * SOIL_DENSITY);

                // Add to soil elevation
                g.H_soil[r][q] += deposited_height;

                // Adjust carrying sediment load
                current_load -= deposited_mass;
            } 
            else if (current_load < capacity && slope > 0.0f) {
                // Erosion: undersaturated flow cuts into the soil / bedrock
                float soil_available = std::max(0.0f, g.H_soil[r][q] - g.H_bedrock[r][q]);
                
                // Bedrock is 40x harder than soil
                float k_eff_soil = k_eff;
                float k_eff_bedrock = k_eff * 0.025f;

                float eroded_height = 0.0f;

                if (soil_available > 0.0f) {
                    float shear_stress_soil = k_eff_soil * std::sqrt(std::max(0.0f, Q_m3s)) * slope;
                    float rate_soil = shear_stress_soil; // m/yr
                    
                    if (rate_soil > 0.0f) {
                        float t_soil = soil_available / rate_soil; // years to erode all soil
                        if (t_soil >= dt) {
                            eroded_height = rate_soil * dt;
                            g.H_soil[r][q] -= eroded_height;
                        } else {
                            eroded_height = soil_available;
                            g.H_soil[r][q] = g.H_bedrock[r][q];
                            
                            float dt_rem = dt - t_soil;
                            float shear_stress_bed = k_eff_bedrock * std::sqrt(std::max(0.0f, Q_m3s)) * slope;
                            float eroded_bed = shear_stress_bed * dt_rem;
                            
                            g.H_soil[r][q] -= eroded_bed;
                            g.H_bedrock[r][q] -= eroded_bed;
                            eroded_height += eroded_bed;
                        }
                    }
                } else {
                    float shear_stress_bed = k_eff_bedrock * std::sqrt(std::max(0.0f, Q_m3s)) * slope;
                    eroded_height = shear_stress_bed * dt;
                    g.H_soil[r][q] -= eroded_height;
                    g.H_bedrock[r][q] -= eroded_height;
                }

                // Add eroded mass to carrying load
                float eroded_mass = eroded_height * hex_area * SOIL_DENSITY;
                current_load += eroded_mass;
            }

            // Write telemetry back to the grid container (normalized to annual rate for visualizer consistency)
            g.sediment[r][q] = (current_load / dt) / (hex_area * SOIL_DENSITY); // local thickness equivalent per year

            // 5. Propagate remaining sediment load to downstream neighbor
            if (next_dir != -1 && nq != -1 && nr != -1) {
                sediment_load[nr][nq] += current_load;
            }
        }
    }
};

#endif // HEX_EROSION_H
