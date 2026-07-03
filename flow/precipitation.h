#ifndef PRECIPITATION_H
#define PRECIPITATION_H

#include "element.h"

// 1. Precipitation Element
class Precipitation : public Element {
private:
    float rain_rate;
public:
    Precipitation(float rate) : rain_rate(rate) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        float cell_rain = rain_rate / (g.size * g.size);
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                g.h_surface[y][x] += cell_rain * dt;
            }
        }
    }
};

#endif // PRECIPITATION_H
