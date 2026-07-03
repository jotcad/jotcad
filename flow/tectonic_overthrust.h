#ifndef TECTONIC_OVERTHRUST_H
#define TECTONIC_OVERTHRUST_H

#include "element.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Tectonic Overthrust Element representing a low-angle thrust fault where one plate slides up and over another
class TectonicOverthrust : public Element {
private:
    float x_a, y_a;
    float x_b, y_b;
    float total_slip;
    float theta_deg; // Ramp angle of the thrust fault (typically 15 to 30 degrees)
    float k;         // Shear boundary transition sharpness

public:
    TectonicOverthrust(float xa, float ya, float xb, float yb, float slip, float angle_deg = 20.0f, float sharpness = 0.5f)
        : x_a(xa), y_a(ya), x_b(xb), y_b(yb), total_slip(slip), theta_deg(angle_deg), k(sharpness) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        float step_slip = total_slip / (float)total_steps;

        float theta_rad = theta_deg * 3.14159265f / 180.0f;
        float slip_h = step_slip * std::cos(theta_rad); // Horizontal component
        float slip_v = step_slip * std::sin(theta_rad); // Vertical uplift component

        float dx = x_b - x_a;
        float dy = y_b - y_a;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;

        // Normal unit vector pointing into the overriding hanging-wall plate (positive distance side)
        float nx = -dy / len;
        float ny = dx / len;

        auto old_soil = g.H_soil;

        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                // Perpendicular signed distance to the plate boundary line
                float dist = ((x_b - x_a) * ((float)y - y_a) - (y_b - y_a) * ((float)x - x_a)) / len;
                
                // Sigmoid factor: 0.0 for footwall plate (subducting/under), 1.0 for hanging-wall plate (overriding)
                float factor = 0.5f + 0.5f * std::tanh(k * dist);

                // Backward lookup coordinates (shift overriding plate in the direction of the normal thrust vector)
                float src_x = (float)x - nx * slip_h * factor;
                float src_y = (float)y - ny * slip_h * factor;

                // Bilinear interpolation
                int x0 = std::floor(src_x);
                int y0 = std::floor(src_y);
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                x0 = std::max(0, std::min(sz - 1, x0));
                x1 = std::max(0, std::min(sz - 1, x1));
                y0 = std::max(0, std::min(sz - 1, y0));
                y1 = std::max(0, std::min(sz - 1, y1));

                float tx = src_x - std::floor(src_x);
                float ty = src_y - std::floor(src_y);

                float h00 = old_soil[y0][x0];
                float h10 = old_soil[y0][x1];
                float h01 = old_soil[y1][x0];
                float h11 = old_soil[y1][x1];

                float interpolated_h = (1.0f - tx) * (1.0f - ty) * h00 +
                                       tx * (1.0f - ty) * h10 +
                                       (1.0f - tx) * ty * h01 +
                                       tx * ty * h11;

                // Set new elevation combining horizontal shift and vertical overthrust ramp climb
                g.H_soil[y][x] = interpolated_h + slip_v * factor;
            }
        }
    }
};

#endif // TECTONIC_OVERTHRUST_H
