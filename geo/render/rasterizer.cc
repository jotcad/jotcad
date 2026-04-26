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

    double x0 = tri.p[0].x * scale + offset_x, y0 = tri.p[0].y * scale + offset_y;
    double x1 = tri.p[1].x * scale + offset_x, y1 = tri.p[1].y * scale + offset_y;
    double x2 = tri.p[2].x * scale + offset_x, y2 = tri.p[2].y * scale + offset_y;

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
            double w0 = edge_func(x1, y1, x2, y2, (double)x, (double)y) / area;
            double w1 = edge_func(x2, y2, x0, y0, (double)x, (double)y) / area;
            double w2 = edge_func(x0, y0, x1, y1, (double)x, (double)y) / area;

            if (w0 >= -1e-6 && w1 >= -1e-6 && w2 >= -1e-6) {
                double depth = w0 * tri.p[0].z + w1 * tri.p[1].z + w2 * tri.p[2].z;
                int idx = y * width + x;
                if (depth >= z_buffer[idx]) {
                    z_buffer[idx] = depth;
                    pixels[idx * 4] = tri.color.r;
                    pixels[idx * 4 + 1] = tri.color.g;
                    pixels[idx * 4 + 2] = tri.color.b;
                    pixels[idx * 4 + 3] = 255;
                }
            }
        }
    }
}

void Rasterizer::rasterize_line(int x0, int y0, int x1, int y1, ColorRGBA col, std::vector<unsigned char>& pixels, int width, int height) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height) {
            int idx = (y0 * width + x0) * 4;
            pixels[idx] = col.r; pixels[idx+1] = col.g; pixels[idx+2] = col.b; pixels[idx+3] = 255;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

std::vector<uint8_t> Rasterizer::render_png(const Geometry& geo, int width, int height, double ax, double ay) {
    std::vector<unsigned char> pixels(width * height * 4, 30);
    for (int i = 3; i < (int)pixels.size(); i += 4) pixels[i] = 255;
    std::vector<double> z_buffer(width * height, -1e18);

    if (geo.vertices.empty()) return {};

    Camera cam(ax, ay);
    std::vector<Vec3> pts;
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const auto& v : geo.vertices) {
        Vec3 p = cam.project(v);
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
    auto add_tri = [&](int i0, int i1, int i2, ColorRGBA base_color) {
        Vec3 p0 = pts[i0], p1 = pts[i1], p2 = pts[i2];
        Vec3 normal = get_normal(p0, p1, p2);
        double brightness = (normal.x + normal.y + normal.z + 1.5) / 4.5 + 0.3;
        ColorRGBA lit_color = {
            (unsigned char)std::min(255.0, base_color.r * brightness),
            (unsigned char)std::min(255.0, base_color.g * brightness),
            (unsigned char)std::min(255.0, base_color.b * brightness),
            base_color.a
        };
        triangles.push_back({{p0, p1, p2}, normal, lit_color, (p0.z + p1.z + p2.z) / 3.0});
    };

    for (const auto& f : geo.faces) {
        Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
            add_tri(i0, i1, i2, {200, 200, 200, 255});
        });
    }

    for (const auto& t : geo.triangles) {
        if (t[0] < (int)pts.size() && t[1] < (int)pts.size() && t[2] < (int)pts.size()) {
            add_tri(t[0], t[1], t[2], {200, 200, 200, 255});
        }
    }

    std::sort(triangles.begin(), triangles.end(), [](const RenderTriangle& a, const RenderTriangle& b) {
        return a.avg_z < b.avg_z;
    });

    for (const auto& tri : triangles) {
        rasterize_triangle(tri, pixels, z_buffer, width, height, scale, offset_x, offset_y);
    }

    for (const auto& s : geo.segments) {
        if (s[0] < (int)pts.size() && s[1] < (int)pts.size()) {
            Vec3 p0 = pts[s[0]], p1 = pts[s[1]];
            rasterize_line((int)(p0.x * scale + offset_x), (int)(p0.y * scale + offset_y),
                           (int)(p1.x * scale + offset_x), (int)(p1.y * scale + offset_y),
                           {255, 255, 0, 255}, pixels, width, height);
        }
    }

    int len;
    unsigned char* png_data = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &len);
    if (!png_data) return {};
    std::vector<uint8_t> result(png_data, png_data + len);
    free(png_data);
    return result;
}

} // namespace geo
} // namespace jotcad
