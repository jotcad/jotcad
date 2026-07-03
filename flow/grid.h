#ifndef GRID_H
#define GRID_H

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>
#include "perlin_noise.h"
#include "fields.h"

// Simulation Grid holding all 2D simulation arrays and a type-safe sparse field registry
struct Grid {
    int size;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> vx; // (size + 1) x size
    std::vector<std::vector<float>> vy; // size x (size + 1)
    std::vector<std::vector<float>> sediment;
    std::vector<std::vector<float>> h_soil_water;

    // Type-safe map of sparse fields, allocated on-demand
    std::unordered_map<std::type_index, std::unique_ptr<std::vector<std::vector<float>>>> sparse_fields;

    Grid(int sz) : size(sz),
        H_soil(sz, std::vector<float>(sz, 0.0f)),
        h_surface(sz, std::vector<float>(sz, 0.0f)),
        vx(sz + 1, std::vector<float>(sz, 0.0f)),
        vy(sz, std::vector<float>(sz + 1, 0.0f)),
        sediment(sz, std::vector<float>(sz, 0.0f)),
        h_soil_water(sz, std::vector<float>(sz, 0.0f)) {}

    // Runtime type-index based field request (used by orchestrator)
    std::vector<std::vector<float>>& request_field_by_type_index(std::type_index id) {
        auto it = sparse_fields.find(id);
        if (it == sparse_fields.end()) {
            sparse_fields[id] = std::make_unique<std::vector<std::vector<float>>>(size, std::vector<float>(size, 0.0f));
            return *sparse_fields[id];
        }
        return *it->second;
    }

    // Compile-time template type-tag field request (used by elements)
    template<typename T>
    std::vector<std::vector<float>>& request_field() {
        return request_field_by_type_index(std::type_index(typeid(T)));
    }

    // Check if a specific sparse field type has been allocated
    template<typename T>
    bool has_field() const {
        return sparse_fields.find(std::type_index(typeid(T))) != sparse_fields.end();
    }

    void initialize_soil_perlin() {
        PerlinNoise2D perlin;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float pn = perlin.noise(x * 0.07f, y * 0.07f) + 0.30f * perlin.noise(x * 0.20f, y * 0.20f);
                H_soil[y][x] = pn * 5.00f;
            }
        }
    }
};

#endif // GRID_H
