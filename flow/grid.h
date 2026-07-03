#ifndef GRID_H
#define GRID_H

#include <vector>
#include <stdexcept>
#include "perlin_noise.h"

// Enum representing the sparse/allocated fields in the geomorphic grid
enum class FieldType {
    SAND,  // Aeolian sand layer
    TILL   // Glacial basal till layer
};

// Simulation Grid holding all 2D simulation arrays
struct Grid {
    int size;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> vx; // (size + 1) x size
    std::vector<std::vector<float>> vy; // size x (size + 1)
    std::vector<std::vector<float>> sediment;
    std::vector<std::vector<float>> h_soil_water;

    // Optional sparse/allocated layers (null by default)
    std::vector<std::vector<float>>* sand = nullptr;
    std::vector<std::vector<float>>* till = nullptr;

    Grid(int sz) : size(sz),
        H_soil(sz, std::vector<float>(sz, 0.0f)),
        h_surface(sz, std::vector<float>(sz, 0.0f)),
        vx(sz + 1, std::vector<float>(sz, 0.0f)),
        vy(sz, std::vector<float>(sz + 1, 0.0f)),
        sediment(sz, std::vector<float>(sz, 0.0f)),
        h_soil_water(sz, std::vector<float>(sz, 0.0f)) {}

    // On-demand field allocator returning reference to flat memory
    std::vector<std::vector<float>>& request_field(FieldType type) {
        if (type == FieldType::SAND) {
            if (!sand) sand = new std::vector<std::vector<float>>(size, std::vector<float>(size, 0.0f));
            return *sand;
        } else if (type == FieldType::TILL) {
            if (!till) till = new std::vector<std::vector<float>>(size, std::vector<float>(size, 0.0f));
            return *till;
        }
        throw std::runtime_error("Unknown field requested");
    }

    // Check if optional field is allocated
    bool has_field(FieldType type) const {
        if (type == FieldType::SAND) return sand != nullptr;
        if (type == FieldType::TILL) return till != nullptr;
        return false;
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

    ~Grid() {
        delete sand;
        delete till;
    }
};

#endif // GRID_H
