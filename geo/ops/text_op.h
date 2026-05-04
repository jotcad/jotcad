#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../render/triangulation.h"
#include "../render/contour_utils.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <vector>
#include <string>
#include <iostream>

namespace jotcad {
namespace geo {

struct OutlineContext {
    std::vector<std::vector<EK::Point_2>> paths;
    EK::Point_2 current_pos;
    FT scale;

    static int move_to(const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        ctx->paths.push_back({});
        ctx->current_pos = EK::Point_2(FT(to->x) * ctx->scale, FT(to->y) * ctx->scale);
        ctx->paths.back().push_back(ctx->current_pos);
        return 0;
    }

    static int line_to(const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        ctx->current_pos = EK::Point_2(FT(to->x) * ctx->scale, FT(to->y) * ctx->scale);
        ctx->paths.back().push_back(ctx->current_pos);
        return 0;
    }

    static int conic_to(const FT_Vector* control, const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        EK::Point_2 p0 = ctx->current_pos;
        EK::Point_2 p1(FT(control->x) * ctx->scale, FT(control->y) * ctx->scale);
        EK::Point_2 p2(FT(to->x) * ctx->scale, FT(to->y) * ctx->scale);
        for (int i = 1; i <= 8; ++i) {
            FT t = FT(i) / 8, invT = 1 - t;
            ctx->current_pos = EK::Point_2(
                invT * invT * p0.x() + 2 * invT * t * p1.x() + t * t * p2.x(),
                invT * invT * p0.y() + 2 * invT * t * p1.y() + t * t * p2.y()
            );
            ctx->paths.back().push_back(ctx->current_pos);
        }
        return 0;
    }

    static int cubic_to(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        EK::Point_2 p0 = ctx->current_pos;
        EK::Point_2 p1(FT(control1->x) * ctx->scale, FT(control1->y) * ctx->scale);
        EK::Point_2 p2(FT(control2->x) * ctx->scale, FT(control2->y) * ctx->scale);
        EK::Point_2 p3(FT(to->x) * ctx->scale, FT(to->y) * ctx->scale);
        for (int i = 1; i <= 8; ++i) {
            FT t = FT(i) / 8, invT = 1 - t;
            ctx->current_pos = EK::Point_2(
                invT * invT * invT * p0.x() + 3 * invT * invT * t * p1.x() + 3 * invT * t * t * p2.x() + t * t * t * p3.x(),
                invT * invT * invT * p0.y() + 3 * invT * invT * t * p1.y() + 3 * invT * t * t * p2.y() + t * t * t * p3.y()
            );
            ctx->paths.back().push_back(ctx->current_pos);
        }
        return 0;
    }
};

template <typename P = JotVfsProtocol>
struct TextOp : P {
    static constexpr const char* path = "jot/text";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::string& text, const nlohmann::json& font_identity, double size) {
        try {
            std::vector<uint8_t> font_data;
            if (font_identity.is_string()) font_data = vfs->read<std::vector<uint8_t>>(fs::CID{font_identity.get<std::string>()});
            else font_data = vfs->read<std::vector<uint8_t>>(font_identity.get<fs::Selector>());
            if (font_data.empty()) throw std::runtime_error("Font data empty");

            FT_Library library;
            if (FT_Init_FreeType(&library)) throw std::runtime_error("FT_Init_FreeType failed");
            FT_Face face;
            if (FT_New_Memory_Face(library, font_data.data(), (FT_Long)font_data.size(), 0, &face)) {
                FT_Done_FreeType(library); throw std::runtime_error("FT_New_Memory_Face failed");
            }
            FT_Set_Pixel_Sizes(face, 0, 64);
            Geometry geo;
            FT x_offset = 0, scale = FT(size) / (64 * 64);
            FT_Outline_Funcs funcs;
            funcs.move_to = OutlineContext::move_to; funcs.line_to = OutlineContext::line_to;
            funcs.conic_to = OutlineContext::conic_to; funcs.cubic_to = OutlineContext::cubic_to;
            funcs.shift = 0; funcs.delta = 0;

            for (char c : text) {
                if (FT_Load_Char(face, c, FT_LOAD_NO_BITMAP)) continue;
                OutlineContext ctx; ctx.scale = scale;
                FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
                std::vector<Polygon> polygons;
                for (auto& raw_path : ctx.paths) {
                    if (raw_path.size() < 3) continue;
                    std::vector<EK::Point_2> clean_path;
                    for (const auto& p : raw_path) {
                        if (clean_path.empty() || CGAL::squared_distance(clean_path.back(), p) > FT(1e-12)) clean_path.push_back(p);
                    }
                    if (clean_path.size() >= 3 && CGAL::squared_distance(clean_path.front(), clean_path.back()) < FT(1e-12)) clean_path.pop_back();
                    if (clean_path.size() < 3) continue;
                    Polygon poly;
                    for (auto& p : clean_path) poly.push_back(EK::Point_2(p.x() + x_offset, p.y()));
                    if (poly.size() >= 3 && poly.is_simple()) polygons.push_back(poly);
                }
                auto groups = ContourUtils::group_polygons(polygons);
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
                x_offset += FT(face->glyph->advance.x) * scale;
            }
            FT_Done_Face(face); FT_Done_FreeType(library);
            vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, geo, {{"type", "text"}}));
        } catch (const std::exception& e) {
            std::cerr << "[TextOp] Error: " << e.what() << std::endl;
            vfs->write(fulfilling.with_output("$out"), Shape());
        }
    }
    static std::vector<std::string> argument_keys() { return {"text", "font", "size"}; }
    static typename P::json schema() {
        return { {"path", "jot/text"}, {"arguments", { {{"name", "text"}, {"type", "jot:string"}}, {{"name", "font"}, {"type", "jot:font"}}, {{"name", "size"}, {"type", "jot:number"}, {"default", 10}} }}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
    }
};

inline void text_init(fs::VFSNode* vfs) {
    Processor::register_op<TextOp<>, std::string, nlohmann::json, double>(vfs, "jot/text");
}

} // namespace geo
} // namespace jotcad
