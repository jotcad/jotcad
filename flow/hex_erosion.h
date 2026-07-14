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

        // Create topological sorting list (highest first)
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

        // Run sediment routing & stream-power erosion
        for (const auto& cell : cells) {
            int q = cell.q;
            int r = cell.r;
            float H_curr = g.H_soil[r][q];

            // 1. Calculate local slope (S) to downstream receiver
            int next_dir = g.downstream_dir[r][q];
            float slope = 0.0f;
            int nq = -1, nr = -1;

            if (next_dir != -1 && g.get_neighbor(q, r, next_dir, nq, nr)) {
                float H_neigh = g.H_soil[nr][nq];
                slope = std::max(0.0f, (H_curr - H_neigh) / dx);
            }

            // 2. Convert cumulative discharge to m^3/s for physical law scaling
            float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;

            // 3. Compute sediment transport capacity (kg/yr): Cap = K_c * Q * S
            // (Units: m^3/s * slope * density * scale)
            float capacity = capacity_coeff * Q_m3s * slope * SOIL_DENSITY * SECONDS_PER_YEAR;

            // 4. Calculate vegetation binding factor (exponential reduction of erodibility)
            float veg = g.vegetation[r][q];
            float k_eff = erodibility * std::exp(-veg_shielding * veg);

            float current_load = sediment_load[r][q];

            if (current_load > capacity) {
                // Deposition: overloaded river deposits excess sediment
                float excess = current_load - capacity;
                float deposited_mass = excess * deposition_rate;
                float deposited_height = deposited_mass / (hex_area * SOIL_DENSITY);

                // Add to soil elevation
                g.H_soil[r][q] += deposited_height;

                // Adjust carrying sediment load
                current_load -= deposited_mass;
            } 
            else if (current_load < capacity && slope > 0.0f) {
                // Erosion: undersaturated flow cuts into the soil
                // Shear stress proxy: tau = k_eff * Q^0.5 * S
                float shear_stress = k_eff * std::sqrt(std::max(0.0f, Q_m3s)) * slope;
                float eroded_height = shear_stress * dt;

                // Subtract from soil elevation
                g.H_soil[r][q] -= eroded_height;

                // Add eroded mass to carrying load
                float eroded_mass = eroded_height * hex_area * SOIL_DENSITY;
                current_load += eroded_mass;
            }

            // Write telemetry back to the grid container
            g.sediment[r][q] = current_load / (hex_area * SOIL_DENSITY); // local thickness equivalent

            // 5. Propagate remaining sediment load to downstream neighbor
            if (next_dir != -1 && nq != -1 && nr != -1) {
                sediment_load[nr][nq] += current_load;
            }
        }
    }
};

#endif // HEX_EROSION_H
