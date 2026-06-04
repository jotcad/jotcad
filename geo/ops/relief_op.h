#pragma once
#include "protocols.h"
#include "processor.h"
#include "../../fs/cpp/vendor/stb_image.h"
#include <vector>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ReliefOp : P {
    static constexpr const char* path = "jot/Relief";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& image_identity, double width = 10.0, double height = 10.0, double depth = 2.0, double base = 1.0, int resolution = 128) {
        try {
            fs::Selector img_sel = image_identity.get<fs::Selector>();
            if (img_sel.output.empty()) img_sel = img_sel.with_output("$out");
            std::vector<uint8_t> img_bytes = vfs->read<std::vector<uint8_t>>(img_sel);
            if (img_bytes.empty()) throw std::runtime_error("Relief: Image data empty");

            int width_px, height_px, channels;
            unsigned char* data = stbi_load_from_memory(img_bytes.data(), (int)img_bytes.size(), &width_px, &height_px, &channels, 3);
            if (!data) throw std::runtime_error("Relief: stbi_load failed");

            // Compute actual working resolution clamped to input image size and parameter limits
            int res_x = resolution;
            int res_y = resolution;
            if (width_px < res_x) res_x = width_px;
            if (height_px < res_y) res_y = height_px;
            if (res_x < 2) res_x = 2;
            if (res_y < 2) res_y = 2;

            std::vector<Vertex> vertices;
            vertices.reserve(res_x * res_y * 2);

            // 1. Generate top surface vertices
            for (int y = 0; y < res_y; ++y) {
                for (int x = 0; x < res_x; ++x) {
                    double u = (res_x > 1) ? (double)x / (res_x - 1) : 0.5;
                    double v = (res_y > 1) ? (double)y / (res_y - 1) : 0.5;

                    int orig_x = std::clamp((int)(u * (width_px - 1)), 0, width_px - 1);
                    int orig_y = std::clamp((int)(v * (height_px - 1)), 0, height_px - 1);
                    int idx = (orig_x + orig_y * width_px) * 3;
                    double r = data[idx] / 255.0;
                    double g = data[idx+1] / 255.0;
                    double b = data[idx+2] / 255.0;
                    double intensity = 0.299 * r + 0.587 * g + 0.114 * b;

                    double px = -width / 2.0 + u * width;
                    double py = -height / 2.0 + v * height;
                    double pz = base + intensity * depth;

                    vertices.push_back(Vertex{FT(px), FT(py), FT(pz)});
                }
            }

            // 2. Generate bottom base vertices
            for (int y = 0; y < res_y; ++y) {
                for (int x = 0; x < res_x; ++x) {
                    double u = (res_x > 1) ? (double)x / (res_x - 1) : 0.5;
                    double v = (res_y > 1) ? (double)y / (res_y - 1) : 0.5;

                    double px = -width / 2.0 + u * width;
                    double py = -height / 2.0 + v * height;
                    double pz = 0.0;

                    vertices.push_back(Vertex{FT(px), FT(py), FT(pz)});
                }
            }

            stbi_image_free(data);

            std::vector<std::array<int, 3>> triangles;
            int offset = res_x * res_y;

            // 3. Top surface triangles
            for (int y = 0; y < res_y - 1; ++y) {
                for (int x = 0; x < res_x - 1; ++x) {
                    int p00 = x + y * res_x;
                    int p10 = (x + 1) + y * res_x;
                    int p01 = x + (y + 1) * res_x;
                    int p11 = (x + 1) + (y + 1) * res_x;

                    triangles.push_back({p00, p10, p01});
                    triangles.push_back({p10, p11, p01});
                }
            }

            // 4. Bottom surface triangles (pointing down)
            for (int y = 0; y < res_y - 1; ++y) {
                for (int x = 0; x < res_x - 1; ++x) {
                    int p00 = x + y * res_x + offset;
                    int p10 = (x + 1) + y * res_x + offset;
                    int p01 = x + (y + 1) * res_x + offset;
                    int p11 = (x + 1) + (y + 1) * res_x + offset;

                    triangles.push_back({p00, p01, p10});
                    triangles.push_back({p10, p01, p11});
                }
            }

            // 5. Side wall triangles
            // Bottom edge (y = 0)
            for (int x = 0; x < res_x - 1; ++x) {
                int t0 = x;
                int t1 = x + 1;
                int b0 = t0 + offset;
                int b1 = t1 + offset;
                triangles.push_back({t0, b0, t1});
                triangles.push_back({t1, b0, b1});
            }

            // Top edge (y = res_y - 1)
            for (int x = 0; x < res_x - 1; ++x) {
                int t0 = x + (res_y - 1) * res_x;
                int t1 = (x + 1) + (res_y - 1) * res_x;
                int b0 = t0 + offset;
                int b1 = t1 + offset;
                triangles.push_back({t0, t1, b0});
                triangles.push_back({t1, b1, b0});
            }

            // Left edge (x = 0)
            for (int y = 0; y < res_y - 1; ++y) {
                int t0 = y * res_x;
                int t1 = (y + 1) * res_x;
                int b0 = t0 + offset;
                int b1 = t1 + offset;
                triangles.push_back({t0, t1, b0});
                triangles.push_back({t1, b1, b0});
            }

            // Right edge (x = res_x - 1)
            for (int y = 0; y < res_y - 1; ++y) {
                int t0 = (res_x - 1) + y * res_x;
                int t1 = (res_x - 1) + (y + 1) * res_x;
                int b0 = t0 + offset;
                int b1 = t1 + offset;
                triangles.push_back({t0, b0, t1});
                triangles.push_back({t1, b0, b1});
            }
            Geometry geo;
            geo.vertices = std::move(vertices);
            geo.triangles = std::move(triangles);

            vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, geo, {}));
        } catch (const std::exception& e) {
            std::cerr << "[ReliefOp] Error: " << e.what() << std::endl;
            vfs->write(fulfilling.with_output("$out"), Shape());
        }
    }

    static std::vector<std::string> argument_keys() { return {"image", "width", "height", "depth", "base", "resolution"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Relief"},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "image"}, {"type", "jot:image"}},
                {{"name", "width"}, {"type", "jot:number"}, {"default", 10.0}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 10.0}},
                {{"name", "depth"}, {"type", "jot:number"}, {"default", 2.0}},
                {{"name", "base"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "resolution"}, {"type", "jot:number"}, {"default", 128}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void relief_init(fs::VFSNode* vfs) {
    Processor::register_op<ReliefOp<>, nlohmann::json, double, double, double, double, int>(vfs, "jot/Relief");
}

} // namespace geo
} // namespace jotcad
