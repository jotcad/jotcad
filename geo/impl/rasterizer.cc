#include "rasterizer.h"
#include <cmath>
#include <algorithm>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace geo {

void Rasterizer::rasterize_triangle(
    Vec3 p0, Vec3 p1, Vec3 p2, Vec3 normal,
    std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
    int width, int height, double scale, double offset_x, double offset_y) {

    // Normal mapping: map [-1, 1] to [0, 255]
    unsigned char r = (unsigned char)((normal.x + 1.0) / 2.0 * 255);
    unsigned char g = (unsigned char)((normal.y + 1.0) / 2.0 * 255);
    unsigned char b = (unsigned char)((normal.z + 1.0) / 2.0 * 255);

    // Screen space coordinates
    double x0 = p0.x * scale + offset_x, y0 = p0.y * scale + offset_y;
    double x1 = p1.x * scale + offset_x, y1 = p1.y * scale + offset_y;
    double x2 = p2.x * scale + offset_x, y2 = p2.y * scale + offset_y;

    // Bounding box
    int minX = (int)std::max(0.0, std::floor(std::min({x0, x1, x2})));
    int maxX = (int)std::min((double)width - 1, std::ceil(std::max({x0, x1, x2})));
    int minY = (int)std::max(0.0, std::floor(std::min({y0, y1, y2})));
    int maxY = (int)std::min((double)height - 1, std::ceil(std::max({y0, y1, y2})));

    auto edge_func = [](double ax, double ay, double bx, double by, double cx, double cy) {
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    };

    double area = edge_func(x0, y0, x1, y1, x2, y2);
    if (std::abs(area) < 1e-6) return;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            double w0 = edge_func(x1, y1, x2, y2, x, y) / area;
            double w1 = edge_func(x2, y2, x0, y0, x, y) / area;
            double w2 = edge_func(x0, y0, x1, y1, x, y) / area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                double depth = w0 * p0.z + w1 * p1.z + w2 * p2.z;
                int idx = y * width + x;
                if (depth > z_buffer[idx]) {
                    z_buffer[idx] = depth;
                    pixels[idx * 3] = r;
                    pixels[idx * 3 + 1] = g;
                    pixels[idx * 3 + 2] = b;
                }
            }
        }
    }
}

std::vector<uint8_t> Rasterizer::render_png(const Geometry& geo, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 3, 30);
    std::vector<double> z_buffer(width * height, -1e18);

    if (geo.vertices.empty()) return {};

    // Isometric rotation
    double ax = 0.61547;
    double ay = 0.78539;
    
    auto project = [&](const Vertex& v) {
        double x = CGAL::to_double(v.x), y = CGAL::to_double(v.y), z = CGAL::to_double(v.z);
        double x1 = x * std::cos(ay) + z * std::sin(ay);
        double z1 = -x * std::sin(ay) + z * std::cos(ay);
        double y2 = y * std::cos(ax) - z1 * std::sin(ax);
        double z2 = y * std::sin(ax) + z1 * std::cos(ax);
        return Vec3{x1, y2, z2};
    };

    std::vector<Vec3> pts;
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const auto& v : geo.vertices) {
        Vec3 p = project(v);
        pts.push_back(p);
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
    }

    double dx = max_x - min_x, dy = max_y - min_y;
    double scale = 0.9 * std::min(width / (dx > 0 ? dx : 1.0), height / (dy > 0 ? dy : 1.0));
    double offset_x = width / 2.0 - (min_x + max_x) / 2.0 * scale;
    double offset_y = height / 2.0 - (min_y + max_y) / 2.0 * scale;

    auto get_normal = [](Vec3 a, Vec3 b, Vec3 c) {
        return (b - a).cross(c - a).normalized();
    };

    // Draw faces
    for (const auto& f : geo.faces) {
        for (const auto& loop : f.loops) {
            if (loop.size() < 3) continue;
            for (size_t i = 1; i < loop.size() - 1; ++i) {
                Vec3 p0 = pts[loop[0]], p1 = pts[loop[i]], p2 = pts[loop[i+1]];
                rasterize_triangle(p0, p1, p2, get_normal(p0, p1, p2), pixels, z_buffer, width, height, scale, offset_x, offset_y);
            }
        }
    }

    // Draw triangles
    for (const auto& t : geo.triangles) {
        Vec3 p0 = pts[t[0]], p1 = pts[t[1]], p2 = pts[t[2]];
        rasterize_triangle(p0, p1, p2, get_normal(p0, p1, p2), pixels, z_buffer, width, height, scale, offset_x, offset_y);
    }

    int len;
    unsigned char* png_data = stbi_write_png_to_mem(pixels.data(), width * 3, width, height, 3, &len);
    if (!png_data) return {};
    std::vector<uint8_t> result(png_data, png_data + len);
    free(png_data);
    return result;
}

} // namespace geo
} // namespace jotcad
