#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../render/contour_utils.h"
#include "../infra/fetch.h"
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include "../../fs/cpp/vendor/stb_image.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TraceOp : P {
    static constexpr const char* path = "jot/trace";

    struct PixelHSV { double h, s, v; };

    static PixelHSV rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b) {
        double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
        double max_v = std::max({rd, gd, bd}), min_v = std::min({rd, gd, bd});
        double h, s, v = max_v, d = max_v - min_v;
        s = (max_v == 0) ? 0 : d / max_v;
        if (max_v == min_v) h = 0;
        else {
            if (max_v == rd) h = (gd - bd) / d + (gd < bd ? 6 : 0);
            else if (max_v == gd) h = (bd - rd) / d + 2;
            else h = (rd - gd) / d + 4;
            h /= 6.0;
        }
        return { h, s, v };
    }

    static double hsv_dist_sq(PixelHSV a, PixelHSV b) {
        double dh = std::min(std::abs(a.h - b.h), 1.0 - std::abs(a.h - b.h));
        double ds = a.s - b.s, dv = a.v - b.v;
        return dh*dh * 4.0 + ds*ds + dv*dv;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const nlohmann::json& image_identity, const std::vector<std::string>& palette) {
        try {
            std::vector<uint8_t> img_bytes;
            if (image_identity.is_string()) img_bytes = vfs->read<std::vector<uint8_t>>(fs::CID{image_identity.get<std::string>()});
            else img_bytes = vfs->read<std::vector<uint8_t>>(image_identity.get<fs::Selector>());

            if (img_bytes.empty()) throw std::runtime_error("Image data empty");

            int width, height, channels;
            unsigned char* data = stbi_load_from_memory(img_bytes.data(), (int)img_bytes.size(), &width, &height, &channels, 3);
            if (!data) throw std::runtime_error("stbi_load failed");

            std::vector<PixelHSV> target_hsv;
            for (const auto& hex : palette) {
                auto rgb = ContourUtils::parse_color(hex);
                target_hsv.push_back(rgb_to_hsv(rgb.r, rgb.g, rgb.b));
            }

            std::vector<int> classification(width * height);
            for (int i = 0; i < width * height; ++i) {
                PixelHSV p = rgb_to_hsv(data[i*3], data[i*3+1], data[i*3+2]);
                int best = 0; double min_d = 1e18;
                for (size_t j = 0; j < target_hsv.size(); ++j) {
                    double d = hsv_dist_sq(p, target_hsv[j]);
                    if (d < min_d) { min_d = d; best = (int)j; }
                }
                classification[i] = best;
            }
            stbi_image_free(data);

            std::vector<Shape> components;
            for (size_t p_idx = 0; p_idx < target_hsv.size(); ++p_idx) {
                std::vector<std::pair<EK::Point_2, EK::Point_2>> segments;
                for (int y = 0; y < height - 1; ++y) {
                    for (int x = 0; x < width - 1; ++x) {
                        int v0 = (classification[y * width + x] == (int)p_idx), v1 = (classification[y * width + (x+1)] == (int)p_idx);
                        int v2 = (classification[(y+1) * width + (x+1)] == (int)p_idx), v3 = (classification[(y+1) * width + x] == (int)p_idx);
                        int case_idx = v0 | (v1 << 1) | (v2 << 2) | (v3 << 3);

                        FT fx(x), fy(y), half = FT(1)/2;
                        EK::Point_2 e0(fx + half, fy), e1(fx + 1, fy + half), e2(fx + half, fy + 1), e3(fx, fy + half);

                        switch (case_idx) {
                            case 1: case 14: segments.push_back({e3, e0}); break;
                            case 2: case 13: segments.push_back({e0, e1}); break;
                            case 3: case 12: segments.push_back({e3, e1}); break;
                            case 4: case 11: segments.push_back({e1, e2}); break;
                            case 5: segments.push_back({e3, e0}); segments.push_back({e1, e2}); break;
                            case 6: case 9:  segments.push_back({e0, e2}); break;
                            case 7: case 8:  segments.push_back({e3, e2}); break;
                            case 10: segments.push_back({e0, e1}); segments.push_back({e2, e3}); break;
                        }
                    }
                }

                auto polygons = ContourUtils::weld_segments(segments);
                auto groups = ContourUtils::group_polygons(polygons);

                Geometry geo;
                for (const auto& group : groups) {
                    Geometry::Face f;
                    auto add_loop = [&](const Polygon& p) {
                        std::vector<int> loop;
                        for (auto it = p.vertices_begin(); it != p.vertices_end(); ++it) {
                            loop.push_back(geo.vertices.size());
                            geo.vertices.push_back({it->x(), it->y(), 0});
                        }
                        return loop;
                    };
                    f.loops.push_back(add_loop(polygons[group.outer]));
                    for (size_t h_idx : group.holes) f.loops.push_back(add_loop(polygons[h_idx]));
                    geo.faces.push_back(f);
                }
                
                components.push_back(P::make_shape(vfs, geo, {{"type", "trace_bucket"}, {"color", palette[p_idx]}}));
            }
            vfs->write(fulfilling.with_output("$out"), Shape::group(components));
        } catch (const std::exception& e) {
            std::cerr << "[TraceOp] Error: " << e.what() << std::endl;
            vfs->write(fulfilling.with_output("$out"), Shape());
        }
    }

    static std::vector<std::string> argument_keys() { return {"image", "palette"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/trace"},
            {"arguments", {
                {{"name", "image"}, {"type", "jot:image"}},
                {{"name", "palette"}, {"type", "jot:strings"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void trace_init(fs::VFSNode* vfs) {
    Processor::register_op<TraceOp<>, nlohmann::json, std::vector<std::string>>(vfs, "jot/trace");
}

} // namespace geo
} // namespace jotcad
