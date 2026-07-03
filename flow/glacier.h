#ifndef GLACIER_H
#define GLACIER_H

#include "element.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Glacier Element representing glacial ice flow, till drag advection, pressure-shadow plastering (drumlins),
// and bedrock abrasion (carving U-shaped valleys, fjords, and parallel linear grooves/striations)
class Glacier : public Element {
private:
    float vx, vy;         // Ice flow velocity vector (direction of glacial movement)
    float deposit_rate;   // Deposition rate in low-pressure zones
    float erode_rate;     // Erosion rate in high-pressure compressional zones
    float drag_rate;      // Basal till dragging/advection speed

public:
    Glacier(float v_x = 0.8f, float v_y = 0.8f, float dep = 0.15f, float er = 0.08f, float drag = 0.12f)
        : vx(v_x), vy(v_y), deposit_rate(dep), erode_rate(er), drag_rate(drag) {}

    std::vector<std::type_index> get_required_fields() const override {
        return { std::type_index(typeid(TillField)) };
    }

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        auto& h_till = g.request_field<TillField>();
        if (step == 0) {
            // Initialize with a uniform thin till sheet
            h_till.assign(sz, std::vector<float>(sz, 0.30f));
        }

        // 1. Basal till drag advection (backwards lookup along ice velocity vector)
        std::vector<std::vector<float>> old_till = h_till;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float src_x = (float)x - vx * drag_rate * dt;
                float src_y = (float)y - vy * drag_rate * dt;

                int x0 = std::max(0, std::min(sz - 1, (int)std::floor(src_x)));
                int y0 = std::max(0, std::min(sz - 1, (int)std::floor(src_y)));
                int x1 = std::max(0, std::min(sz - 1, x0 + 1));
                int y1 = std::max(0, std::min(sz - 1, y0 + 1));

                float tx = src_x - std::floor(src_x);
                float ty = src_y - std::floor(src_y);

                h_till[y][x] = (1.0f - tx) * (1.0f - ty) * old_till[y0][x0] +
                               tx * (1.0f - ty) * old_till[y0][x1] +
                               (1.0f - tx) * ty * old_till[y1][x0] +
                               tx * ty * old_till[y1][x1];
            }
        }

        // 2. Till erosion and pressure-shadow plastering
        for (int y = 1; y < sz - 1; ++y) {
            for (int x = 1; x < sz - 1; ++x) {
                // Bedrock slopes in X and Y
                float dh_dx = (g.H_soil[y][x+1] - g.H_soil[y][x-1]) * 0.5f;
                float dh_dy = (g.H_soil[y+1][x] - g.H_soil[y-1][x]) * 0.5f;

                // Dot product of ice flow with bedrock slope:
                // Positive means ice flows uphill (facing flow -> compression / erosion)
                // Negative means ice flows downhill (shadow zone -> expansion / deposition)
                float grad = vx * dh_dx + vy * dh_dy;

                if (grad > 0.0f) {
                    // Upstream compressional zone: erosion of till
                    float loss = erode_rate * grad * h_till[y][x] * dt;
                    h_till[y][x] = std::max(0.0f, h_till[y][x] - loss);
                } else {
                    // Downstream pressure shadow zone: deposition/plastering of till
                    float gain = deposit_rate * (-grad) * dt;
                    h_till[y][x] += gain;
                }
            }
        }

        // 3. Bedrock Abrasion (grinding down solid bedrock H_soil to carve U-shaped fjords and parallel grooves)
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                // Abrasion is highest where till is present as grinding tools,
                // but too much till (cushioning) or no till (clean ice) drops abrasion to zero.
                float till_sandpaper_factor = h_till[y][x] * std::exp(-h_till[y][x] / 0.40f); 
                float velocity_magnitude = std::sqrt(vx * vx + vy * vy);

                // Add a high-frequency spatial scratcher along the transverse axis (X-axis)
                // to carve long, parallel linear grooves (striations) along the flow direction (Y-axis)
                // Using overlapping waves to simulate random boulders gouging paths
                float scratch = 1.0f + 0.85f * std::sin(x * 1.55f) * std::cos(x * 0.85f);

                // Bedrock wear rate: grinds away bedrock height H_soil
                float abrasion_rate = 0.09f * velocity_magnitude * till_sandpaper_factor * scratch;
                g.H_soil[y][x] = std::max(0.0f, g.H_soil[y][x] - abrasion_rate * dt);
            }
        }

        // Expose till layer depth to surface depth for unified visualization
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                g.h_surface[y][x] = h_till[y][x];
            }
        }
    }
};

#endif // GLACIER_H
