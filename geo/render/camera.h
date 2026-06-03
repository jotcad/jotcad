#pragma once
#include <cmath>
#include <algorithm>
#include "geometry.h"

namespace jotcad {
namespace geo {

struct Vec2 {
    double x, y;
};

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

struct Camera {
    double ax, ay; // Rotation angles

    Camera(double ax = 0.0, double ay = 0.0) : ax(ax), ay(ay) {}

    Vec3 project(const Vertex& v) const {
        double x = CGAL::to_double(v.x), y = CGAL::to_double(v.y), z = CGAL::to_double(v.z);
        // Standard rotation matrix application
        double x1 = x * std::cos(ay) + z * std::sin(ay);
        double z1 = -x * std::sin(ay) + z * std::cos(ay);
        double y2 = y * std::cos(ax) - z1 * std::sin(ax);
        double z2 = y * std::sin(ax) + z1 * std::cos(ax);
        return Vec3{x1, y2, z2};
    }
};

} // namespace geo
} // namespace jotcad
