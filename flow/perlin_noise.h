#ifndef PERLIN_NOISE_H
#define PERLIN_NOISE_H

#include <cmath>

// Simple Perlin Noise generator for procedural terrain
class PerlinNoise2D {
private:
    int p[512];
    float fade(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
    float lerp(float t, float a, float b) {
        return a + t * (b - a);
    }
    float grad(int hash, float x, float y) {
        int h = hash & 3;
        float u = h < 2 ? x : -x;
        float v = (h == 0 || h == 2) ? y : -y;
        return u + v;
    }
public:
    PerlinNoise2D() {
        static const int permutation[256] = {
            151,160,137,91,90,15,
            131,13,201,95,96,53,194,233, 7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
            190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
            88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
            77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
            102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
            135,130,116,188,189,128,1,119,248,73,188,206,120,244,214,107,252,24,178,160,
            182,235,180,186,13,250,134,124,142,29,166,224,198,129,73,81,244,61,63,67,112,
            81,60,183,190,137,254,121,78,184,182,220,25,97,252,90,121,39,182,182,245,63,
            11,186,198,186,112,112,85,124,190,132,60,183,191,159,22,219,94,223,254,233,
            10,23,22,252,232,154,254, 52,59,228,174,178,174,20,204,188,110,60,190,184,
            186,197,186,120,116,117,144,22,120,120,150,160,143,159,34,53,60,182,182,204,
            68,171,200,172,144,144,149,150,149,150,228,174,188,110,60,184
        };
        for (int i = 0; i < 256; ++i) {
            p[i] = permutation[i];
            p[256 + i] = permutation[i];
        }
    }
    float noise(float x, float y) {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        float u = fade(x);
        float v = fade(y);
        int aa = p[p[X] + Y];
        int ab = p[p[X] + Y + 1];
        int ba = p[p[X + 1] + Y];
        int bb = p[p[X + 1] + Y + 1];
        float x1 = lerp(u, grad(aa, x, y), grad(ba, x - 1.0f, y));
        float x2 = lerp(u, grad(ab, x, y - 1.0f), grad(bb, x - 1.0f, y - 1.0f));
        return lerp(v, x1, x2);
    }
};

#endif // PERLIN_NOISE_H
