#include "rasterizer.h"
#include <cmath>
#include <algorithm>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace geo {

void Rasterizer::rasterize_triangle(
    const RenderTriangle& tri,
    std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
    int width, int height, double scale, double offset_x, double offset_y) {

    // Screen space coordinates
    double x0 = tri.p[0].x * scale + offset_x, y0 = tri.p[0].y * scale + offset_y;
    double x1 = tri.p[1].x * scale + offset_x, y1 = tri.p[1].y * scale + offset_y;
    double x2 = tri.p[2].x * scale + offset_x, y2 = tri.p[2].y * scale + offset_y;

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
                double depth = w0 * tri.p[0].z + w1 * tri.p[1].z + w2 * tri.p[2].z;
                int idx = y * width + x;
                
                if (depth >= z_buffer[idx]) {
                    if (tri.color.a == 255) {
                        z_buffer[idx] = depth;
                        pixels[idx * 4] = tri.color.r;
                        pixels[idx * 4 + 1] = tri.color.g;
                        pixels[idx * 4 + 2] = tri.color.b;
                        pixels[idx * 4 + 3] = 255;
                    } else {
                        // Alpha blending: destination is current pixel
                        double alpha = tri.color.a / 255.0;
                        pixels[idx * 4] = (unsigned char)(tri.color.r * alpha + pixels[idx * 4] * (1.0 - alpha));
                        pixels[idx * 4 + 1] = (unsigned char)(tri.color.g * alpha + pixels[idx * 4 + 1] * (1.0 - alpha));
                        pixels[idx * 4 + 2] = (unsigned char)(tri.color.b * alpha + pixels[idx * 4 + 2] * (1.0 - alpha));
                    }
                }
            }
        }
    }
}

std::vector<uint8_t> Rasterizer::render_png(const Geometry& geo, int width, int height, double ax, double ay) {
    // 4 channels (RGBA), background is dark gray
    std::vector<unsigned char> pixels(width * height * 4, 30);
    for (int i = 3; i < pixels.size(); i += 4) pixels[i] = 255; // Solid Alpha
    std::vector<double> z_buffer(width * height, -1e18);

    if (geo.vertices.empty()) return {};

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

    std::vector<RenderTriangle> triangles;
    auto add_tri = [&](Vec3 p0, Vec3 p1, Vec3 p2, ColorRGBA base_color) {
        Vec3 normal = get_normal(p0, p1, p2);
        // Basic lighting based on normal
        double brightness = (normal.x + normal.y + normal.z + 1.5) / 4.5 + 0.3;
        ColorRGBA lit_color = {
            (unsigned char)std::min(255.0, base_color.r * brightness),
            (unsigned char)std::min(255.0, base_color.g * brightness),
            (unsigned char)std::min(255.0, base_color.b * brightness),
            base_color.a
        };
        double avg_z = (p0.z + p1.z + p2.z) / 3.0;
        triangles.push_back({{p0, p1, p2}, normal, lit_color, avg_z});
    };

    // Draw faces
    for (const auto& f : geo.faces) {
        ColorRGBA col = {200, 200, 200, 255};
        for (const auto& loop : f.loops) {
            if (loop.size() < 3) continue;
            for (size_t i = 1; i < loop.size() - 1; ++i) {
                add_tri(pts[loop[0]], pts[loop[i]], pts[loop[i+1]], col);
            }
        }
    }

    // Draw triangles
    for (const auto& t : geo.triangles) {
        add_tri(pts[t[0]], pts[t[1]], pts[t[2]], {200, 200, 200, 255});
    }

    // BACK-TO-FRONT SORTING (Mandatory for Alpha Blending)
    std::sort(triangles.begin(), triangles.end(), [](const RenderTriangle& a, const RenderTriangle& b) {
        return a.avg_z < b.avg_z; // Painter's Algorithm
    });

    for (const auto& tri : triangles) {
        rasterize_triangle(tri, pixels, z_buffer, width, height, scale, offset_x, offset_y);
    }

    int len;
    // Note: STBI write with 4 channels
    unsigned char* png_data = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &len);
    if (!png_data) return {};
    std::vector<uint8_t> result(png_data, png_data + len);
    free(png_data);
    return result;
}

} // namespace geo
} // namespace jotcad
