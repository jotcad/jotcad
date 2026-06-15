#pragma once
#include <vector>
#include <string>
#include "geometry.h"
#include "camera.h"
#include "triangulation.h"
#include "../core/protocols.h"

namespace jotcad {
namespace geo {

struct ColorRGBA {
    unsigned char r, g, b, a;
};

struct Texture {
    int width, height, channels;
    std::vector<unsigned char> pixels;
    
    ColorRGBA sample(double u, double v) const {
        if (pixels.empty()) return {255, 255, 255, 255};
        // Repeat wrapping
        int x = (int)(std::floor(u * width)) % width;
        int y = (int)(std::floor(v * height)) % height;
        if (x < 0) x += width;
        if (y < 0) y += height;
        int idx = (y * width + x) * channels;
        if (channels == 3) return {pixels[idx], pixels[idx+1], pixels[idx+2], 255};
        if (channels == 4) return {pixels[idx], pixels[idx+1], pixels[idx+2], pixels[idx+3]};
        return {255, 255, 255, 255};
    }
};

class Rasterizer {
public:
    /**
     * render_png: Standard JotCAD rasterizer.
     * Traverses a Shape hierarchy, resolving geometry via VFS and applying
     * color tags to produce a depth-buffered PNG.
     */
    static std::vector<uint8_t> render_png(
        fs::VFSNode* vfs,
        const Shape& shape,
        int width = 256, int height = 256, double ax = 0.0, double ay = 0.0);

private:
    struct RenderTriangle {
        Vec3 p[3];
        Vec2 uv[3];
        Vec3 normal;
        ColorRGBA color;
        const Texture* texture = nullptr;
        double avg_z;
    };

    struct RenderLine {
        Vec3 p0, p1;
        ColorRGBA color;
    };

    static void rasterize_triangle(
        const RenderTriangle& tri,
        std::vector<unsigned char>& pixels, std::vector<double>& z_buffer,
        int width, int height, double scale, double offset_x, double offset_y);

    static void rasterize_line(
        int x0, int y0, int x1, int y1, ColorRGBA col,
        std::vector<unsigned char>& pixels, int width, int height);
};

} // namespace geo
} // namespace jotcad
