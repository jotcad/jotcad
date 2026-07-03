#ifndef NOISE_H
#define NOISE_H

#include "element.h"
#include "perlin_noise.h"

// Noise Element for adding procedural sub-grid micro-roughness or weathering noise
class NoiseElement : public Element {
private:
    float frequency;
    float amplitude;
    bool accumulate;
public:
    NoiseElement(float freq, float amp, bool accum = false) 
        : frequency(freq), amplitude(amp), accumulate(accum) {}

    void step(Grid& g, float dt, int step, int total_steps) override {
        PerlinNoise2D perlin;
        for (int y = 0; y < g.size; ++y) {
            for (int x = 0; x < g.size; ++x) {
                float noise_val = perlin.noise(x * frequency, y * frequency);
                if (accumulate) {
                    g.H_soil[y][x] += noise_val * amplitude * dt;
                } else {
                    g.H_soil[y][x] += noise_val * amplitude;
                }
            }
        }
    }
};

#endif // NOISE_H
