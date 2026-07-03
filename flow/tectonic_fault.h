#ifndef TECTONIC_FAULT_H
#define TECTONIC_FAULT_H

#include "element.h"
#include <cmath>

// Tectonic Fault Element representing vertical dip-slip displacement along a line segment
class TectonicFault : public Element {
private:
    float x_a, y_a;
    float x_b, y_b;
    float H_throw; // Total slip vertical throw
    float k;       // Sharpness parameter of fault zone

public:
    TectonicFault(float xa, float ya, float xb, float yb, float throw_val, float sharpness = 1.0f)
        : x_a(xa), y_a(ya), x_b(xb), y_b(yb), H_throw(throw_val), k(sharpness) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        float step_throw = H_throw / (float)total_steps;

        float dx = x_b - x_a;
        float dy = y_b - y_a;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                // Compute signed perpendicular distance to segment AB
                float dist = ((x_b - x_a) * ((float)y - y_a) - (y_b - y_a) * ((float)x - x_a)) / len;
                // Sigmoid deformation field
                float factor = 1.0f / (1.0f + std::exp(-k * dist));
                g.H_soil[y][x] += step_throw * (factor - 0.5f); // Center offset around 0
            }
        }
    }
};

#endif // TECTONIC_FAULT_H
