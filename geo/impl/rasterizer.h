#pragma once
#include <vector>
#include <string>
#include <cmath>
#include "geometry.h"

namespace jotcad {
namespace geo {

struct Vec3 {
    double x, y, z;
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3& other) const { return x * other.x + y * other.y + z * other.z; }
    Vec3 cross(const Vec3& other) const {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const {
        double len = length();
        return len > 0 ? Vec3{x / len, y / len, z / len} : Vec3{0, 0, 0};
    }
};

struct ColorRGBA {
    unsigned char r, g, b, a;
};

class Rasterizer {
public:
    static std::vector<uint8_t> render_png(const Geometry& geo, int width = 256, int height = 256);

private:
    struct RenderTriangle {
        Vec3 p[3];
        Vec3 normal;
        ColorRGBA color;
        double avg_z;
    };

    static void rasterize_triangle(
        const RenderTriangle& tri,
        std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
        int width, int height, double scale, double offset_x, double offset_y);
};

} // namespace geo
} // namespace jotcad
