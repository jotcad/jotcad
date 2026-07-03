#ifndef PRECIPITATION_H
#define PRECIPITATION_H

#include "element.h"

// 1. Precipitation Element
class Precipitation : public Element {
private:
    float rain_rate;
    bool scale_invariant;
public:
    Precipitation(float rate, bool invariant = false) : rain_rate(rate), scale_invariant(invariant) {}
    void step(Grid& g, float dt, int step, int total_steps) override {
        if (scale_invariant) {
            // Scale-invariant mode: directly adds precipitation depth (configured in mm per time unit)
            float rain_meters = rain_rate / 1000.0f;
            for (int y = 0; y < g.size; ++y) {
                for (int x = 0; x < g.size; ++x) {
                    g.h_surface[y][x] += rain_meters * dt;
                }
            }
        } else {
            // Legacy volumetric mode (divided by grid area)
            float cell_rain = rain_rate / (g.size * g.size);
            for (int y = 0; y < g.size; ++y) {
                for (int x = 0; x < g.size; ++x) {
                    g.h_surface[y][x] += cell_rain * dt;
                }
            }
        }
    }
};

#endif // PRECIPITATION_H
