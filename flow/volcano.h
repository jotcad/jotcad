#ifndef VOLCANO_H
#define VOLCANO_H

#include "element.h"
#include "perlin_noise.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Volcano Element representing viscous lava flow eruption, transport, and solidification
class Volcano : public Element {
private:
    int vent_x, vent_y;
    float eruption_rate;
    float viscosity_drag;
    float cooling_rate;
    float solidification_rate;
    std::vector<std::vector<float>> h_lava;

public:
    Volcano(int vx, int vy, float er, float drag, float cool, float solid)
        : vent_x(vx), vent_y(vy), eruption_rate(er), viscosity_drag(drag), cooling_rate(cool), solidification_rate(solid) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        int sz = g.size;
        if (h_lava.empty()) {
            h_lava.assign(sz, std::vector<float>(sz, 0.0f));
        }

        // 1. Eruption at vent center
        h_lava[vent_y][vent_x] += eruption_rate * dt;

        // 2. Viscous flow driven by hydrostatic slope (8-neighborhood lubrication flow)
        std::vector<std::vector<float>> z_total(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                z_total[y][x] = g.H_soil[y][x] + h_lava[y][x];
            }
        }

        // Compute local viscosity mask using Perlin noise to simulate cooling crust irregularities
        PerlinNoise2D perlin;
        std::vector<std::vector<float>> local_visc(sz, std::vector<float>(sz, 0.0f));
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                float noise_val = perlin.noise(x * 0.18f, y * 0.18f); // high-frequency cooling noise
                local_visc[y][x] = viscosity_drag * (1.0f + 0.65f * noise_val);
            }
        }

        std::vector<std::vector<float>> next_lava = h_lava;
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (h_lava[y][x] < 0.001f) continue;

                int dx[] = {1, -1, 0, 0, 1, -1, 1, -1};
                int dy[] = {0, 0, 1, -1, 1, 1, -1, -1};
                float dist[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.4142f, 1.4142f, 1.4142f, 1.4142f};
                
                for (int i = 0; i < 8; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    if (nx >= 0 && nx < sz && ny >= 0 && ny < sz) {
                        float slope = (z_total[y][x] - z_total[ny][nx]) / dist[i];
                        if (slope > 0.0f) {
                            // Flow rate uses averaged local viscosity between cells
                            float avg_drag = (local_visc[y][x] + local_visc[ny][nx]) * 0.5f;
                            float flow = (h_lava[y][x] * h_lava[y][x] * h_lava[y][x]) * slope * avg_drag * dt / dist[i];
                            flow = std::min(flow, h_lava[y][x] * 0.12f); // flux limit
                            next_lava[y][x] -= flow;
                            next_lava[ny][nx] += flow;
                        }
                    }
                }
            }
        }
        h_lava = next_lava;

        // 3. Cooling & Solidification (lava volume freezes into H_soil bedrock)
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                if (h_lava[y][x] > 0.001f) {
                    float solid = h_lava[y][x] * solidification_rate * dt;
                    solid = std::min(solid, h_lava[y][x]);
                    g.H_soil[y][x] += solid;
                    h_lava[y][x] -= solid;
                }
            }
        }

        // Expose lava depth to surface depth for unified visualization
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                g.h_surface[y][x] = h_lava[y][x];
            }
        }
    }
};

#endif // VOLCANO_H
