#ifndef VEGETATION_H
#define VEGETATION_H

#include "element.h"
#include "perlin_noise.h"
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
        return { std::type_index(typeid(GrassField)), std::type_index(typeid(TreeField)), std::type_index(typeid(SoilField)) };
    }

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        auto& grass = g.request_field<GrassField>();
        auto& tree = g.request_field<TreeField>();
        auto& soil = g.request_field<SoilField>();
        PerlinNoise2D perlin;

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

                // In-situ soil weathering: bedrock slowly weathers into soil on dry land (where not flooded)
                if (!is_flooded) {
                    soil[y][x] += 0.0008f * dt; // Weathering rate: balanced for 5000 cycles
                }

                // Soil requirements for vegetation
                float soil_thickness_m = soil[y][x] * g.scale.height_scale_m;
                float grass_soil_factor = std::min(1.0f, soil_thickness_m / 0.05f); // Requires 5 cm (0.05 m) of soil
                float tree_soil_factor = (soil_thickness_m < 0.08f) ? -0.15f : std::min(1.0f, soil_thickness_m / 0.25f); // Trees require 25 cm (0.25 m) of soil, decay if < 8 cm
 
                // Beach constraint: trees cannot grow in the active wave-washed beach zone (near deep ocean water)
                bool is_beach = false;
                if (g.h_surface[y][x] <= 0.02f) {
                    for (int dy = -2; dy <= 2 && !is_beach; ++dy) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            int ny = y + dy;
                            int nx = x + dx;
                            if (ny >= 0 && ny < sz && nx >= 0 && nx < sz) {
                                if (g.h_surface[ny][nx] > 0.45f) {
                                    is_beach = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (is_beach || is_flooded) {
                    tree_soil_factor = -0.20f; // Trees decay in loose, salty beach sand or in flooded river channels
                }

                // 3. Logistic Growth equations
                if (soil_support > 0.05f) {
                    // GRASS GROWTH
                    // Grass colonizes bare soil/sand if seeded
                    if (grass[y][x] < 0.01f) grass[y][x] += 0.01f * dt;
                    float grass_growth = grass_growth_rate * grass[y][x] * (carrying_capacity - grass[y][x]) * moisture * grass_soil_factor * dt;
                    grass[y][x] = std::max(0.0f, std::min(carrying_capacity, grass[y][x] + grass_growth));

                    // TREE GROWTH
                    // Trees grow slower, and grass competes with tree saplings
                    if (tree[y][x] < 0.005f && grass[y][x] > 0.05f && tree_soil_factor > 0.0f) {
                        tree[y][x] += 0.002f * dt;
                    }
                    float tree_cap = carrying_capacity - grass[y][x] * 0.35f; // Grass crowd-out factor
                    if (moisture < 0.35f) {
                        tree_cap = std::min(tree_cap, 0.22f); // Savanna constraint: scattered tree groves (22% capacity) on dry plains
                    }
                    float tree_growth;
                    if (tree_soil_factor < 0.0f) {
                        tree_growth = tree_soil_factor * tree[y][x] * dt; // Decay due to lack of soil anchor
                    } else {
                        tree_growth = tree_growth_rate * tree[y][x] * (tree_cap - tree[y][x]) * moisture * tree_soil_factor * dt;
                    }
                    tree[y][x] = std::max(0.0f, std::min(carrying_capacity, tree[y][x] + tree_growth));
                }

                // 4. Environmental Destruction
                // - Marine saltwater death: terrestrial plants cannot grow below sea level in the ocean
                if (g.H_soil[y][x] <= 0.0f && is_flooded) {
                    grass[y][x] = 0.0f;
                    tree[y][x] = 0.0f;
                }

                // - Submersion death: deep standing water kills land grass slowly
                float water_depth_m = water_depth * g.scale.height_scale_m;
                if (is_flooded && water_depth_m > 0.50f) {
                    grass[y][x] = std::max(0.0f, grass[y][x] - 0.15f * dt);
                    tree[y][x] = std::max(0.0f, tree[y][x] - 0.05f * dt); // Trees survive longer
                }

                // - Hydraulic erosion shear stress: fast-flowing water rips out vegetation (boundary safe)
                if (is_flooded) {
                    float vx_val = (g.vx[x][y] + g.vx[x+1][y]) * 0.5f;
                    float vy_val = (g.vy[x][y] + g.vy[x][y+1]) * 0.5f;
                    float velocity_mag = std::sqrt(vx_val*vx_val + vy_val*vy_val);
 
                    if (velocity_mag > 4000.0f) {
                        float shear_damage = 0.00009f * (velocity_mag - 4000.0f) * dt;
                        grass[y][x] = std::max(0.0f, grass[y][x] - shear_damage);
                        tree[y][x] = std::max(0.0f, tree[y][x] - 0.4f * shear_damage);
                    }
                }

                // - Sand burial: thick sand cover suppresses shallow grass, but trees can grow out of it
                if (sand && (*sand)[y][x] > 0.85f) {
                    float burial_factor = ((*sand)[y][x] - 0.85f) * 0.20f * dt;
                    grass[y][x] = std::max(0.01f, grass[y][x] - burial_factor);
                }

                // 5. Continuous Biogenic Bioturbation: churn bedrock and soil to maintain micro-scale roughness texture
                if (!is_flooded && g.H_soil[y][x] > -1.0f) {
                    float noise_val = perlin.noise(x * 0.45f + step * 0.05f, y * 0.45f + step * 0.05f);
                    g.H_soil[y][x] += noise_val * 0.00004f * (dt / 0.05f);
                }
            }
        }
    }
};

#endif // VEGETATION_H
