#ifndef LANDSLIDE_H
#define LANDSLIDE_H

#include "element.h"
#include <cmath>
#include <algorithm>

// 4. Landslide Element (Terrain Collapse Repose using 8-neighbor sweeps)
class Landslide : public Element {
private:
    float max_slope;
    int sink_x;
    int sink_y;
public:
    Landslide(float slope, int sx = 74, int sy = 49) : max_slope(slope), sink_x(sx), sink_y(sy) {}
    
    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;

        // Cache optional vegetation and soil pointers once per step
        const std::vector<std::vector<float>>* grass_ptr = g.has_field<GrassField>() ? &g.request_field<GrassField>() : nullptr;
        const std::vector<std::vector<float>>* tree_ptr = g.has_field<TreeField>() ? &g.request_field<TreeField>() : nullptr;
        std::vector<std::vector<float>>* soil_ptr = g.has_field<SoilField>() ? &g.request_field<SoilField>() : nullptr;

        for (int sweep = 0; sweep < 3; ++sweep) {
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    // Non-Erodible Boundaries: Skip outer edges
                    if (x == 0 || x == sz - 1 || y == 0 || y == sz - 1) continue;
                    // Skip internal drain
                    if (x == sink_x && y == sink_y) continue;

                    float h_center = g.H_soil[y][x];
                    
                    // 8 neighbors
                    int dx[8] = {0, 0, -1, 1, -1, 1, -1, 1};
                    int dy[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
                    float dist[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f};

                    for (int i = 0; i < 8; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                            if (nx == 0 || nx == sz - 1 || ny == 0 || ny == sz - 1) continue;
                            if (nx == sink_x && ny == sink_y) continue;

                            float h_neighbor = g.H_soil[ny][nx];
                            float slope = (h_center - h_neighbor) / dist[i];
                            
                            // Root cohesion increases local stable repose slope threshold
                            float local_max_slope = max_slope;
                            if (grass_ptr || tree_ptr) {
                                float grass_val = grass_ptr ? (*grass_ptr)[y][x] : 0.0f;
                                float tree_val = tree_ptr ? (*tree_ptr)[y][x] : 0.0f;
                                // Grass adds +12%, deep tree roots add +42% stable slope threshold
                                local_max_slope *= (1.0f + 0.12f * grass_val + 0.42f * tree_val);
                            }

                            if (slope > local_max_slope) {
                                float excess = (slope - local_max_slope) * 0.15f;
                                float mass = excess * (dist[i] * 0.5f); // mass-conserving slope relaxation
                                g.H_soil[y][x] -= mass;
                                g.H_soil[ny][nx] += mass;
                                h_center -= mass;
                                
                                // Slide soil downslope proportionally with landslide mass
                                if (soil_ptr) {
                                    float soil_to_move = std::min((*soil_ptr)[y][x], mass);
                                    (*soil_ptr)[y][x] -= soil_to_move;
                                    (*soil_ptr)[ny][nx] += soil_to_move;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif // LANDSLIDE_H
