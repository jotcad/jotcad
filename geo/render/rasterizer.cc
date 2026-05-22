#include "rasterizer.h"
#include <cmath>
#include <algorithm>
#include "contour_utils.h"
#include "matrix.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../fs/cpp/vendor/stb_image_write.h"

namespace jotcad {
namespace geo {

void Rasterizer::rasterize_triangle(
    const RenderTriangle& tri,
    std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
    int width, int height, double scale, double offset_x, double offset_y) {

    double x0 = tri.p[0].x * scale + offset_x, y0 = (height - 1) - (tri.p[0].y * scale + offset_y);
    double x1 = tri.p[1].x * scale + offset_x, y1 = (height - 1) - (tri.p[1].y * scale + offset_y);
    double x2 = tri.p[2].x * scale + offset_x, y2 = (height - 1) - (tri.p[2].y * scale + offset_y);

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

std::vector<uint8_t> Rasterizer::render_png(fs::VFSNode* vfs, const Shape& shape, int width, int height, double ax, double ay) {
    std::vector<unsigned char> pixels(width * height * 4, 30);
    for (int i = 3; i < (int)pixels.size(); i += 4) pixels[i] = 255;
    std::vector<double> z_buffer(width * height, -1e18);

    Camera cam(ax, ay);
    std::vector<RenderTriangle> triangles;
    std::vector<std::pair<std::pair<Vec3, Vec3>, ColorRGBA>> wireframe;

    // 1. Scene Collection
    auto collect = [&](auto self, const Shape& s, const std::string& current_color) -> void {
        Matrix current_tf = s.tf;
        std::string next_color = s.tags.value("color", current_color);

        if (s.geometry.has_value() && !s.is_gap()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            ColorRGBA base_color = {200, 200, 200, 255};
            if (!next_color.empty()) {
                auto rgb = ContourUtils::parse_color(next_color);
                base_color = {rgb.r, rgb.g, rgb.b, 255};
            }

            std::vector<Vec3> pts;
            for (const auto& v : geo.vertices) {
                Point_3 p_world = current_tf.t.transform(Point_3(v.x, v.y, v.z));
                pts.push_back(cam.project(Vertex{p_world.x(), p_world.y(), p_world.z()}));
            }

            auto add_tri = [&](int i0, int i1, int i2) {
                Vec3 p0 = pts[i0], p1 = pts[i1], p2 = pts[i2];
                Vec3 normal = (p1 - p0).cross(p2 - p0).normalized();
                double brightness = (normal.x + normal.y + normal.z + 1.5) / 4.5 + 0.3;
                ColorRGBA lit_color = {
                    (unsigned char)std::min(255.0, base_color.r * brightness),
                    (unsigned char)std::min(255.0, base_color.g * brightness),
                    (unsigned char)std::min(255.0, base_color.b * brightness),
                    base_color.a
                };
                triangles.push_back({{p0, p1, p2}, normal, lit_color, (p0.z + p1.z + p2.z) / 3.0});
            };

            for (const auto& f : geo.faces) Triangulation::triangulate_face(f, pts, add_tri);
            for (const auto& t : geo.triangles) add_tri(t[0], t[1], t[2]);
            for (const auto& s_wire : geo.segments) {
                wireframe.push_back({{pts[s_wire[0]], pts[s_wire[1]]}, {255, 255, 0, 255}});
            }
        }
        for (const auto& child : s.components) self(self, child, next_color);
    };

    collect(collect, shape, "");
    if (triangles.empty() && wireframe.empty()) return {};

    // 2. Global View Setup
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    for (const auto& tri : triangles) {
        for (int i = 0; i < 3; ++i) {
            min_x = std::min(min_x, tri.p[i].x); max_x = std::max(max_x, tri.p[i].x);
            min_y = std::min(min_y, tri.p[i].y); max_y = std::max(max_y, tri.p[i].y);
        }
    }
    for (const auto& wf : wireframe) {
        min_x = std::min({min_x, wf.first.first.x, wf.first.second.x});
        max_x = std::max({max_x, wf.first.first.x, wf.first.second.x});
        min_y = std::min({min_y, wf.first.first.y, wf.first.second.y});
        max_y = std::max({max_y, wf.first.first.y, wf.first.second.y});
    }

    double scale = 0.9 * std::min(width / (max_x - min_x + 1e-6), height / (max_y - min_y + 1e-6));
    double offset_x = width / 2.0 - (min_x + max_x) / 2.0 * scale;
    double offset_y = height / 2.0 - (min_y + max_y) / 2.0 * scale;

    // 3. Rasterization
    std::sort(triangles.begin(), triangles.end(), [](const RenderTriangle& a, const RenderTriangle& b) {
        return a.avg_z < b.avg_z;
    });

    for (const auto& tri : triangles) {
        rasterize_triangle(tri, pixels, z_buffer, width, height, scale, offset_x, offset_y);
    }
    for (const auto& wf : wireframe) {
        rasterize_line((int)(wf.first.first.x * scale + offset_x), (int)((height - 1) - (wf.first.first.y * scale + offset_y)),
                       (int)(wf.first.second.x * scale + offset_x), (int)((height - 1) - (wf.first.second.y * scale + offset_y)),
                       wf.second, pixels, width, height);
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
