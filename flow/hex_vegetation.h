#ifndef HEX_VEGETATION_H
#define HEX_VEGETATION_H

#include "hex_element.h"
#include <algorithm>
#include <vector>
#include <cmath>

// High-level Vegetation Growth & Destruction Element for HexGrid
class HexVegetation : public HexElement {
private:
    float growth_rate;        // Base growth rate of vegetation (r)
    float carrying_capacity; // Maximum vegetation density (default 1.0f)
    float rain_threshold;    // Minimum annual rainfall in meters for full growth

public:
    HexVegetation(float rate = 0.15f, float cap = 1.0f, float rain_thresh = 0.8f)
        : growth_rate(rate), carrying_capacity(cap), rain_threshold(rain_thresh) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        int sq = g.size_q;
        int sr = g.size_r;

        const float SECONDS_PER_YEAR = 31557600.0f;

        // Initialize vegetation on step 0
        if (step == 0) {
            for (int r = 0; r < sr; ++r) {
                for (int q = 0; q < sq; ++q) {
                    if (g.H_soil[r][q] > 0.0f) {
                        g.vegetation[r][q] = 0.05f; // Sparse seeding
                    } else {
                        g.vegetation[r][q] = 0.0f;
                    }
                }
            }
        }

        for (int r = 0; r < sr; ++r) {
            for (int q = 0; q < sq; ++q) {
                float H = g.H_soil[r][q];

                // 1. Compute moisture availability
                // High river discharge or surface water increases local moisture
                // Convert annual flow volume to water depth equivalent
                float moisture = 0.20f; // Background moisture
                float annual_rain = g.h_surface[r][q]; // precip is written here initially by HexPrecipitation

                if (annual_rain > 0.01f) {
                    moisture = std::min(1.0f, annual_rain / rain_threshold);
                }

                // River channels get extra moisture but also high discharge scouring
                float Q_m3s = g.Q[r][q] / SECONDS_PER_YEAR;
                if (Q_m3s > 10.0f) {
                    moisture = std::max(moisture, 0.8f); // Riparian moisture enhancement
                }

                // 2. Compute dynamic carrying capacity under physical limits
                float soil_thickness = H - g.H_bedrock[r][q];
                auto& h_lake = g.request_field<HexLakeDepth>();
                float water_depth = g.h_surface[r][q] + h_lake[r][q];
                float effective_cap = carrying_capacity;

                if (water_depth > 0.50f) {
                    // Drowning Limit (standing water depth > 50 cm)
                    effective_cap = 0.0f;
                } else if (soil_thickness < 0.05f) {
                    // Bare Bedrock Limit (0 to 5 cm soil)
                    effective_cap = 0.0f;
                } else if (soil_thickness < 0.40f) {
                    // Grass/Shrub Limit (5 to 40 cm soil)
                    effective_cap = 0.40f * ((soil_thickness - 0.05f) / 0.35f);
                } else if (soil_thickness < 1.00f) {
                    // Forest Establishment Limit (40 cm to 1.0 m soil)
                    effective_cap = 0.40f + 0.60f * ((soil_thickness - 0.40f) / 0.60f);
                }

                // Altitude-temperature cooling growth constraint (treeline / alpine zone limit)
                float temp = 25.0f - (H / 85.0f);
                float temp_factor = std::max(0.0f, std::min(1.0f, (temp - 10.0f) / 8.0f));
                effective_cap *= temp_factor;

                // 3. Logistic growth (Stable analytical integration)
                float V = g.vegetation[r][q];
                if (effective_cap > 0.0f) {
                    float V_0 = std::max(0.01f, V); // Seed start density
                    float r = growth_rate * moisture;
                    float r_dt = r * dt;

                    if (r_dt > 20.0f) {
                        // Safe limit to prevent exponential overflow, immediately reaches capacity
                        V = effective_cap;
                    } else {
                        float exp_rt = std::exp(r_dt);
                        V = (effective_cap * V_0 * exp_rt) / (effective_cap + V_0 * (exp_rt - 1.0f));
                    }
                } else {
                    // Rapid decay to 0 under drowning/bedrock stress
                    V = std::max(0.0f, V - 0.5f * dt);
                }

                // 4. Hydraulic channel clearing
                // High river flows sweep away vegetation inside the main channels
                if (Q_m3s > 50.0f) {
                    float shear_damage = 0.002f * (Q_m3s - 50.0f) * dt;
                    V = std::max(0.0f, V - shear_damage);
                }

                g.vegetation[r][q] = V;

                // 5. Dual-Directional Soil Production (Pedogenesis & Carbon Capture)
                if (V > 0.0f) {
                    float current_soil_thickness = g.H_soil[r][q] - g.H_bedrock[r][q];
                    
                    // A. Bedrock Weathering (Bottom-Up): converts rock to mineral soil, lowering bedrock boundary
                    float P_0 = 0.00005f; // Potential weathering rate: 0.05 mm/yr
                    float k = 2.0f;       // Thickness decay constant: 2.0 m^-1
                    float weathering = P_0 * (0.1f + 0.9f * V) * std::exp(-k * current_soil_thickness) * dt;
                    g.H_bedrock[r][q] -= weathering;

                    // B. Organic Accumulation (Top-Down): leaf litter and humus build-up, raising surface elevation
                    float R_org = 0.00002f; // Potential organic accumulation rate: 0.02 mm/yr
                    float organic_accumulation = R_org * V * dt;
                    g.H_soil[r][q] += organic_accumulation;
                }
            }
        }
    }
};

#endif // HEX_VEGETATION_H
