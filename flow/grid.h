#ifndef GRID_H
#define GRID_H

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <stdexcept>
#include "perlin_noise.h"
#include "fields.h"

struct ScaleConfig {
    float cell_spacing_m = 100.0f; // cell size in meters
    float height_scale_m = 500.0f; // vertical elevation multiplier in meters
};

// Simulation Grid holding all 2D simulation arrays and a type-safe sparse field registry
struct Grid {
    int size;
    ScaleConfig scale;
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

    void upscale_in_place(int target_size) {
        if (size == target_size) return;
        
        int src_size = size;
        scale.cell_spacing_m *= (float)src_size / (float)target_size;
        auto src_H = H_soil;
        auto src_h = h_surface;
        auto src_sed = sediment;
        auto src_sw = h_soil_water;
        
        std::unordered_map<std::type_index, std::vector<std::vector<float>>> src_sparse;
        for (auto& pair : sparse_fields) {
            src_sparse[pair.first] = *pair.second;
        }
        
        size = target_size;
        H_soil = std::vector<std::vector<float>>(target_size, std::vector<float>(target_size, 0.0f));
        h_surface = std::vector<std::vector<float>>(target_size, std::vector<float>(target_size, 0.0f));
        vx = std::vector<std::vector<float>>(target_size + 1, std::vector<float>(target_size, 0.0f));
        vy = std::vector<std::vector<float>>(target_size, std::vector<float>(target_size + 1, 0.0f));
        sediment = std::vector<std::vector<float>>(target_size, std::vector<float>(target_size, 0.0f));
        h_soil_water = std::vector<std::vector<float>>(target_size, std::vector<float>(target_size, 0.0f));
        
        sparse_fields.clear();
        
        float scale = (float)src_size / (float)target_size;
        for (int y = 0; y < target_size; ++y) {
            for (int x = 0; x < target_size; ++x) {
                float src_x = x * scale;
                float src_y = y * scale;

                int x0 = std::floor(src_x);
                int y0 = std::floor(src_y);
                int x1 = std::min(src_size - 1, x0 + 1);
                int y1 = std::min(src_size - 1, y0 + 1);

                float tx = src_x - x0;
                float ty = src_y - y0;

                H_soil[y][x] = (1.0f - tx) * (1.0f - ty) * src_H[y0][x0] +
                               tx * (1.0f - ty) * src_H[y0][x1] +
                               (1.0f - tx) * ty * src_H[y1][x0] +
                               tx * ty * src_H[y1][x1];

                h_surface[y][x] = (1.0f - tx) * (1.0f - ty) * src_h[y0][x0] +
                                  tx * (1.0f - ty) * src_h[y0][x1] +
                                  (1.0f - tx) * ty * src_h[y1][x0] +
                                  tx * ty * src_h[y1][x1];

                sediment[y][x] = (1.0f - tx) * (1.0f - ty) * src_sed[y0][x0] +
                                 tx * (1.0f - ty) * src_sed[y0][x1] +
                                 (1.0f - tx) * ty * src_sed[y1][x0] +
                                 tx * ty * src_sed[y1][x1];

                h_soil_water[y][x] = (1.0f - tx) * (1.0f - ty) * src_sw[y0][x0] +
                                     tx * (1.0f - ty) * src_sw[y0][x1] +
                                     (1.0f - tx) * ty * src_sw[y1][x0] +
                                     tx * ty * src_sw[y1][x1];
            }
        }
        
        for (auto& pair : src_sparse) {
            auto& dst_field = request_field_by_type_index(pair.first);
            auto& src_field = pair.second;
            
            for (int y = 0; y < target_size; ++y) {
                for (int x = 0; x < target_size; ++x) {
                    float src_x = x * scale;
                    float src_y = y * scale;
                    int x0 = std::floor(src_x);
                    int y0 = std::floor(src_y);
                    int x1 = std::min(src_size - 1, x0 + 1);
                    int y1 = std::min(src_size - 1, y0 + 1);
                    float tx = src_x - x0;
                    float ty = src_y - y0;
                    
                    dst_field[y][x] = (1.0f - tx) * (1.0f - ty) * src_field[y0][x0] +
                                      tx * (1.0f - ty) * src_field[y0][x1] +
                                      (1.0f - tx) * ty * src_field[y1][x0] +
                                      tx * ty * src_field[y1][x1];
                }
            }
        }
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
