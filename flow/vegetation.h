#ifndef VEGETATION_H
#define VEGETATION_H

#include "element.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdlib>

// Vegetation Element representing grass and tree growth, water/moisture dependencies,
// and flood/burial destruction processes running concurrently within a simulation phase
class Vegetation : public Element {
private:
    float grass_growth_rate; // Base growth rate of grass
    float tree_growth_rate;  // Base growth rate of trees
    float carrying_capacity; // Maximum vegetation density (1.0)

public:
    Vegetation(float grass_rate = 0.22f, float tree_rate = 0.08f, float cap = 1.0f)
        : grass_growth_rate(grass_rate), tree_growth_rate(tree_rate), carrying_capacity(cap) {}

    std::vector<std::type_index> get_required_fields() const override {
        return { std::type_index(typeid(GrassField)), std::type_index(typeid(TreeField)) };
    }

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        auto& grass = g.request_field<GrassField>();
        auto& tree = g.request_field<TreeField>();

        // Retrieve optional sediment layers if present using type-tag keys
        const std::vector<std::vector<float>>* sand = g.has_field<SandField>() ? &g.request_field<SandField>() : nullptr;
        const std::vector<std::vector<float>>* till = g.has_field<TillField>() ? &g.request_field<TillField>() : nullptr;

        // Initialize grass and trees on step 0
        if (step == 0) {
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    // Seed initial sparse grass and trees
                    grass[y][x] = 0.05f;
                    tree[y][x] = 0.01f;
                }
            }
        }

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                // 1. Determine Soil/Sediment availability
                float active_sediment = 0.0f;
                if (sand) active_sediment += (*sand)[y][x];
                if (till) active_sediment += (*till)[y][x];
                
                // Base weathered bedrock soil support
                float soil_support = 0.25f + active_sediment;

                // 2. Determine Moisture availability
                float moisture = 0.20f; // Default background moisture
                if (g.h_soil_water[y][x] > 0.01f) {
                    moisture = std::min(1.20f, 0.20f + g.h_soil_water[y][x] * 1.50f);
                }

                // Check standing water (if h_surface represents water depth in a hydrological phase)
                float water_depth = 0.0f;
                bool is_flooded = false;
                
                // Active layer is water ONLY if we are NOT in an active sand dune phase
                if (!sand) {
                    water_depth = g.h_surface[y][x];
                    if (water_depth > 0.01f) {
                        is_flooded = true;
                        if (water_depth < 0.25f) {
                            moisture = 1.20f; // Shallow standing water is ideal (wetlands)
                        } else {
                            moisture = 0.05f; // Deep water suffocates plants (no growth)
                        }
                    }
                }

                // 3. Logistic Growth equations
                if (soil_support > 0.05f) {
                    // GRASS GROWTH
                    // Grass colonizes bare soil/sand if seeded
                    if (grass[y][x] < 0.01f) grass[y][x] += 0.01f * dt;
                    float grass_growth = grass_growth_rate * grass[y][x] * (carrying_capacity - grass[y][x]) * moisture * dt;
                    grass[y][x] = std::max(0.0f, std::min(carrying_capacity, grass[y][x] + grass_growth));

                    // TREE GROWTH
                    // Trees grow slower, and grass competes with tree saplings
                    if (tree[y][x] < 0.005f && grass[y][x] > 0.05f) tree[y][x] += 0.002f * dt;
                    float tree_cap = carrying_capacity - grass[y][x] * 0.35f; // Grass crowd-out factor
                    float tree_growth = tree_growth_rate * tree[y][x] * (tree_cap - tree[y][x]) * moisture * dt;
                    tree[y][x] = std::max(0.0f, std::min(carrying_capacity, tree[y][x] + tree_growth));
                }

                // 4. Environmental Destruction
                // - Submersion death: deep standing water kills land grass slowly
                if (is_flooded && water_depth > 0.50f) {
                    grass[y][x] = std::max(0.0f, grass[y][x] - 0.15f * dt);
                    tree[y][x] = std::max(0.0f, tree[y][x] - 0.05f * dt); // Trees survive longer
                }

                // - Hydraulic erosion shear stress: fast-flowing water rips out vegetation (boundary safe)
                if (is_flooded) {
                    float vx_val = (x < sz - 1) ? 0.5f * (g.vx[y][x] + g.vx[y][x+1]) : g.vx[y][x];
                    float vy_val = (y < sz - 1) ? 0.5f * (g.vy[y][x] + g.vy[y+1][x]) : g.vy[y][x];
                    float velocity_mag = std::sqrt(vx_val*vx_val + vy_val*vy_val);

                    if (velocity_mag > 1.2f) {
                        float shear_damage = 0.30f * (velocity_mag - 1.2f) * dt;
                        grass[y][x] = std::max(0.0f, grass[y][x] - shear_damage);
                        tree[y][x] = std::max(0.0f, tree[y][x] - 0.4f * shear_damage);
                    }
                }

                // - Sand burial: thick sand cover suppresses shallow grass, but trees can grow out of it
                if (sand && (*sand)[y][x] > 0.85f) {
                    float burial_factor = ((*sand)[y][x] - 0.85f) * 0.20f * dt;
                    grass[y][x] = std::max(0.01f, grass[y][x] - burial_factor);
                }
            }
        }
    }
};

#endif // VEGETATION_H
