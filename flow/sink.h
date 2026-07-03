#ifndef SINK_H
#define SINK_H

#include "element.h"
#include <algorithm>

// 5. Internal Sink Element with Mass Balance telemetries
class Sink : public Element {
private:
    int sink_x;
    int sink_y;
public:
    double total_water_removed = 0.0;
    double total_sediment_removed = 0.0;
    double last_cycle_water_removed = 0.0;
    double last_cycle_sediment_removed = 0.0;

    Sink(int sx, int sy) : sink_x(sx), sink_y(sy) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        // Adjust sink cell to always be lowest to pull gravity flows
        float min_neighbor = std::min({
            g.H_soil[sink_y-1][sink_x], g.H_soil[sink_y+1][sink_x],
            g.H_soil[sink_y][sink_x-1], g.H_soil[sink_y][sink_x+1]
        });
        float old_h = g.H_soil[sink_y][sink_x];
        float new_h = min_neighbor - 0.01f;
        g.H_soil[sink_y][sink_x] = new_h;

        // Drain water and sediment
        total_water_removed += g.h_surface[sink_y][sink_x];
        double sediment_excavated = g.sediment[sink_y][sink_x] + (old_h - new_h);
        total_sediment_removed += sediment_excavated;

        if (step == total_steps - 1) {
            last_cycle_water_removed = g.h_surface[sink_y][sink_x];
            last_cycle_sediment_removed = sediment_excavated;
        }

        g.h_surface[sink_y][sink_x] = 0.0f;
        g.h_soil_water[sink_y][sink_x] = 0.0f;
        g.sediment[sink_y][sink_x] = 0.0f;
    }
};

#endif // SINK_H
