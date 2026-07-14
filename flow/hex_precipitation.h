#ifndef HEX_PRECIPITATION_H
#define HEX_PRECIPITATION_H

#include "hex_element.h"

// Precipitation Element for the hexagonal grid
class HexPrecipitation : public HexElement {
private:
    float rain_rate; // Rainfall rate in meters per year (m/yr)
public:
    HexPrecipitation(float rate) : rain_rate(rate) {}

    void step(HexGrid& g, float dt, int step, int total_steps) override {
        // rain_rate * dt gives the rainfall depth in meters added during this time-step
        float rain_depth = rain_rate * dt;
        for (int r = 0; r < g.size_r; ++r) {
            for (int q = 0; q < g.size_q; ++q) {
                g.h_surface[r][q] += rain_depth;
            }
        }
    }
};

#endif // HEX_PRECIPITATION_H
