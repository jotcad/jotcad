#ifndef LANDSLIDE_H
#define LANDSLIDE_H

#include "element.h"

// 4. Landslide Element (Terrain Collapse Repose)
class Landslide : public Element {
private:
    float max_slope;
    int sink_x;
    int sink_y;
public:
    Landslide(float slope, int sx = 74, int sy = 49) : max_slope(slope), sink_x(sx), sink_y(sy) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        for (int sweep = 0; sweep < 3; ++sweep) {
            for (int y = 0; y < sz; ++y) {
                for (int x = 0; x < sz; ++x) {
                    // Non-Erodible Boundaries: Skip outer edges
                    if (x == 0 || x == sz - 1 || y == 0 || y == sz - 1) continue;
                    // Skip internal drain
                    if (x == sink_x && y == sink_y) continue;

                    float h_center = g.H_soil[y][x];
                    int dx[4] = {0, 0, -1, 1};
                    int dy[4] = {-1, 1, 0, 0};

                    for (int i = 0; i < 4; ++i) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                            if (nx == 0 || nx == sz - 1 || ny == 0 || ny == sz - 1) continue;
                            if (nx == sink_x && ny == sink_y) continue;

                            float h_neighbor = g.H_soil[ny][nx];
                            float diff = h_center - h_neighbor;
                            if (diff > max_slope) {
                                float excess = (diff - max_slope) * 0.15f;
                                g.H_soil[y][x] -= excess;
                                g.H_soil[ny][nx] += excess;
                                h_center -= excess;
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif // LANDSLIDE_H
