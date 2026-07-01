#pragma once
#include <vector>
#include <numeric>
#include <random>
#include <cmath>
#include <algorithm>

class PerlinNoise {
private:
    std::vector<int> p;
    double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    double lerp(double t, double a, double b) { return a + t * (b - a); }
    double grad(int hash, double x, double y) {
        int h = hash & 7;
        double u = h < 4 ? x : y;
        double v = h < 4 ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
public:
    PerlinNoise(unsigned int seed = 12345) {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);
        p.insert(p.end(), p.begin(), p.end());
    }
    double noise(double x, double y) {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        double u = fade(x);
        double v = fade(y);
        int A = p[X] + Y;
        int B = p[X + 1] + Y;
        return lerp(v, lerp(u, grad(p[A], x, y), grad(p[B], x - 1, y)),
                       lerp(u, grad(p[A + 1], x, y - 1), grad(p[B + 1], x - 1, y - 1)));
    }
    double fBm(double x, double y, int octaves, double lacunarity, double gain) {
        double total = 0.0;
        double amplitude = 1.0;
        double frequency = 1.0;
        double maxValue = 0.0;
        for (int i = 0; i < octaves; ++i) {
            total += noise(x * frequency, y * frequency) * amplitude;
            maxValue += amplitude;
            amplitude *= gain;
            frequency *= lacunarity;
        }
        return total / maxValue;
    }
    double ridged(double x, double y, int octaves, double lacunarity, double gain) {
        double total = 0.0;
        double amplitude = 1.0;
        double frequency = 1.0;
        double weight = 1.0;
        for (int i = 0; i < octaves; ++i) {
            double n = noise(x * frequency, y * frequency);
            n = std::abs(n);
            n = 1.0 - n;
            n = n * n;
            n *= weight;
            total += n * amplitude;
            weight = std::clamp(n * 2.0, 0.0, 1.0);
            amplitude *= gain;
            frequency *= lacunarity;
        }
        return total;
    }
};
