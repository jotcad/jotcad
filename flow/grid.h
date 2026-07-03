#ifndef GRID_H
#define GRID_H

#include <vector>
#include "perlin_noise.h"

// Simulation Grid holding all 2D simulation arrays
struct Grid {
    int size;
    std::vector<std::vector<float>> H_soil;
    std::vector<std::vector<float>> h_surface;
    std::vector<std::vector<float>> vx; // (size + 1) x size
    std::vector<std::vector<float>> vy; // size x (size + 1)
    std::vector<std::vector<float>> sediment;
    std::vector<std::vector<float>> h_soil_water;

    Grid(int sz) : size(sz),
        H_soil(sz, std::vector<float>(sz, 0.0f)),
        h_surface(sz, std::vector<float>(sz, 0.0f)),
        vx(sz + 1, std::vector<float>(sz, 0.0f)),
        vy(sz, std::vector<float>(sz + 1, 0.0f)),
        sediment(sz, std::vector<float>(sz, 0.0f)),
        h_soil_water(sz, std::vector<float>(sz, 0.0f)) {}

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
