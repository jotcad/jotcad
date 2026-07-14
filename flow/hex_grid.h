#ifndef HEX_GRID_H
#define HEX_GRID_H

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cmath>
#include <stdexcept>
#include "fields.h"

struct HexScaleConfig {
    float hex_radius_m = 5000.0f; // 5 km hex radius (center to vertex)
    float height_scale_m = 500.0f; // vertical scaling factor in meters
};

// Neighbor axial offset directions:
// Directions 0 to 5 represent: East, North-East, North-West, West, South-West, South-East
// in axial coordinates (q, r).
const int HEX_DQ[6] = {1, 1, 0, -1, -1, 0};
const int HEX_DR[6] = {0, -1, -1, 0, 1, 1};

// Dynamic sparse field tag for standing lake depths
struct HexLakeDepth {};

struct HexGrid {
    int size_q;
    int size_r;
    HexScaleConfig scale;

    // Core cell-centered state variables
    std::vector<std::vector<float>> H_soil;      // Total ground elevation (bedrock + soil) (m)
    std::vector<std::vector<float>> H_bedrock;   // Bedrock floor elevation (m)
    std::vector<std::vector<float>> h_surface;   // Surface water depth (m)
    std::vector<std::vector<float>> Q;           // Accumulated annual flow discharge (m^3/yr)
    std::vector<std::vector<float>> sediment;    // Suspended sediment capacity/load
    std::vector<std::vector<float>> vegetation;  // Vegetation density (0.0 to 1.0)
    
    // Flow routing connectivity: direction (0 to 5) of downstream cell, or -1 if sink
    std::vector<std::vector<int>> downstream_dir;

    // Type-safe map of sparse fields, allocated on-demand
    std::unordered_map<std::type_index, std::unique_ptr<std::vector<std::vector<float>>>> sparse_fields;

    HexGrid(int sq, int sr) : size_q(sq), size_r(sr),
        H_soil(sr, std::vector<float>(sq, 0.0f)),
        H_bedrock(sr, std::vector<float>(sq, 0.0f)),
        h_surface(sr, std::vector<float>(sq, 0.0f)),
        Q(sr, std::vector<float>(sq, 0.0f)),
        sediment(sr, std::vector<float>(sq, 0.0f)),
        vegetation(sr, std::vector<float>(sq, 0.0f)),
        downstream_dir(sr, std::vector<int>(sq, -1)) {}

    // Check if coordinates are within the grid bounds
    bool is_valid(int q, int r) const {
        return q >= 0 && q < size_q && r >= 0 && r < size_r;
    }

    // Get the neighbor cell in the specified direction (0-5)
    bool get_neighbor(int q, int r, int dir, int& nq, int& nr) const {
        if (dir < 0 || dir >= 6) return false;
        nq = q + HEX_DQ[dir];
        nr = r + HEX_DR[dir];
        return is_valid(nq, nr);
    }

    // Convert axial coordinates (q, r) to Cartesian (x, y) for pointy-topped hexagons
    void get_cartesian_coords(int q, int r, float& x, float& y) const {
        x = scale.hex_radius_m * 1.7320508f * (q + r * 0.5f);
        y = scale.hex_radius_m * 1.5f * r;
    }

    // Runtime type-index based field request (used by orchestrator)
    std::vector<std::vector<float>>& request_field_by_type_index(std::type_index id) {
        auto it = sparse_fields.find(id);
        if (it == sparse_fields.end()) {
            sparse_fields[id] = std::make_unique<std::vector<std::vector<float>>>(size_r, std::vector<float>(size_q, 0.0f));
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

    // Bilinear-equivalent upscaling of the hexagonal grid (re-interpolating elevations & state)
    void upscale_in_place(int target_q, int target_r) {
        if (size_q == target_q && size_r == target_r) return;

        int src_q = size_q;
        int src_r = size_r;
        
        // Scale hex radius down to match resolution increase
        scale.hex_radius_m *= (float)src_q / (float)target_q;

        auto src_H = H_soil;
        auto src_h = h_surface;
        auto src_Q = Q;
        auto src_sed = sediment;
        auto src_veg = vegetation;
        auto src_down = downstream_dir;

        std::unordered_map<std::type_index, std::vector<std::vector<float>>> src_sparse;
        for (auto& pair : sparse_fields) {
            src_sparse[pair.first] = *pair.second;
        }

        size_q = target_q;
        size_r = target_r;

        H_soil = std::vector<std::vector<float>>(target_r, std::vector<float>(target_q, 0.0f));
        h_surface = std::vector<std::vector<float>>(target_r, std::vector<float>(target_q, 0.0f));
        Q = std::vector<std::vector<float>>(target_r, std::vector<float>(target_q, 0.0f));
        sediment = std::vector<std::vector<float>>(target_r, std::vector<float>(target_q, 0.0f));
        vegetation = std::vector<std::vector<float>>(target_r, std::vector<float>(target_q, 0.0f));
        downstream_dir = std::vector<std::vector<int>>(target_r, std::vector<int>(target_q, -1));

        sparse_fields.clear();

        float scale_q = (float)src_q / (float)target_q;
        float scale_r = (float)src_r / (float)target_r;

        for (int r = 0; r < target_r; ++r) {
            for (int q = 0; q < target_q; ++q) {
                float src_val_q = q * scale_q;
                float src_val_r = r * scale_r;

                int q0 = std::floor(src_val_q);
                int r0 = std::floor(src_val_r);
                int q1 = std::min(src_q - 1, q0 + 1);
                int r1 = std::min(src_r - 1, r0 + 1);

                float tq = src_val_q - q0;
                float tr = src_val_r - r0;

                H_soil[r][q] = (1.0f - tq) * (1.0f - tr) * src_H[r0][q0] +
                               tq * (1.0f - tr) * src_H[r0][q1] +
                               (1.0f - tq) * tr * src_H[r1][q0] +
                               tq * tr * src_H[r1][q1];

                h_surface[r][q] = (1.0f - tq) * (1.0f - tr) * src_h[r0][q0] +
                                  tq * (1.0f - tr) * src_h[r0][q1] +
                                  (1.0f - tq) * tr * src_h[r1][q0] +
                                  tq * tr * src_h[r1][q1];

                Q[r][q] = (1.0f - tq) * (1.0f - tr) * src_Q[r0][q0] +
                          tq * (1.0f - tr) * src_Q[r0][q1] +
                          (1.0f - tq) * tr * src_Q[r1][q0] +
                          tq * tr * src_Q[r1][q1];

                sediment[r][q] = (1.0f - tq) * (1.0f - tr) * src_sed[r0][q0] +
                                 tq * (1.0f - tr) * src_sed[r0][q1] +
                                 (1.0f - tq) * tr * src_sed[r1][q0] +
                                 tq * tr * src_sed[r1][q1];

                vegetation[r][q] = (1.0f - tq) * (1.0f - tr) * src_veg[r0][q0] +
                                   tq * (1.0f - tr) * src_veg[r0][q1] +
                                   (1.0f - tq) * tr * src_veg[r1][q0] +
                                   tq * tr * src_veg[r1][q1];
                
                // Map downstream direction to the nearest coordinate's routing
                int sq = std::round(src_val_q);
                int sr = std::round(src_val_r);
                sq = std::max(0, std::min(src_q - 1, sq));
                sr = std::max(0, std::min(src_r - 1, sr));
                downstream_dir[r][q] = src_down[sr][sq];
            }
        }

        for (auto& pair : src_sparse) {
            auto& dst_field = request_field_by_type_index(pair.first);
            auto& src_field = pair.second;

            for (int r = 0; r < target_r; ++r) {
                for (int q = 0; q < target_q; ++q) {
                    float src_val_q = q * scale_q;
                    float src_val_r = r * scale_r;
                    int q0 = std::floor(src_val_q);
                    int r0 = std::floor(src_val_r);
                    int q1 = std::min(src_q - 1, q0 + 1);
                    int r1 = std::min(src_r - 1, r0 + 1);
                    float tq = src_val_q - q0;
                    float tr = src_val_r - r0;

                    dst_field[r][q] = (1.0f - tq) * (1.0f - tr) * src_field[r0][q0] +
                                      tq * (1.0f - tr) * src_field[r0][q1] +
                                      (1.0f - tq) * tr * src_field[r1][q0] +
                                      tq * tr * src_field[r1][q1];
                }
            }
        }
    }
};

#endif // HEX_GRID_H
