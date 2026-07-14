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

                // 1. Saltwater marine death (below sea level)
                if (H <= 0.0f) {
                    g.vegetation[r][q] = 0.0f;
                    continue;
                }

                // 2. Compute moisture availability
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

                // 3. Logistic growth
                float V = g.vegetation[r][q];
                if (V < 0.01f) V += 0.01f * dt; // Seeding growth
                
                float growth = growth_rate * V * (carrying_capacity - V) * moisture * dt;
                V = std::max(0.0f, std::min(carrying_capacity, V + growth));

                // 4. Hydraulic channel clearing
                // High river flows sweep away vegetation inside the main channels
                if (Q_m3s > 50.0f) {
                    float shear_damage = 0.002f * (Q_m3s - 50.0f) * dt;
                    V = std::max(0.0f, V - shear_damage);
                }

                g.vegetation[r][q] = V;
            }
        }
    }
};

#endif // HEX_VEGETATION_H
