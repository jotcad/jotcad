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
    static constexpr const char* path = "jot/Text";
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

            std::vector<uint32_t> codepoints;
            size_t i_str = 0;
            while (i_str < text.size()) {
                uint32_t cp = 0;
                unsigned char c = (unsigned char)text[i_str];
                if (c <= 0x7F) {
                    cp = c; i_str += 1;
                } else if ((c & 0xE0) == 0xC0) {
                    if (i_str + 1 < text.size()) cp = ((c & 0x1F) << 6) | ((unsigned char)text[i_str+1] & 0x3F);
                    i_str += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    if (i_str + 2 < text.size()) cp = ((c & 0x0F) << 12) | (((unsigned char)text[i_str+1] & 0x3F) << 6) | ((unsigned char)text[i_str+2] & 0x3F);
                    i_str += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    if (i_str + 3 < text.size()) cp = ((c & 0x07) << 18) | (((unsigned char)text[i_str+1] & 0x3F) << 12) | (((unsigned char)text[i_str+2] & 0x3F) << 6) | ((unsigned char)text[i_str+3] & 0x3F);
                    i_str += 4;
                } else {
                    i_str += 1;
                }
                codepoints.push_back(cp);
            }

            for (uint32_t cp : codepoints) {
                FT_UInt glyph_idx = FT_Get_Char_Index(face, cp);
                if (glyph_idx == 0) continue;
                if (FT_Load_Glyph(face, glyph_idx, FT_LOAD_NO_BITMAP)) continue;
                OutlineContext ctx; ctx.scale = scale;
                FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
                std::vector<EK::Segment_2> segments;
                for (auto& raw_path : ctx.paths) {
                    if (raw_path.size() < 2) continue;
                    std::vector<EK::Point_2> clean_path;
                    for (const auto& p : raw_path) {
                        EK::Point_2 p_off(p.x() + x_offset, p.y());
                        if (clean_path.empty() || CGAL::squared_distance(clean_path.back(), p_off) > FT(1e-12)) {
                            clean_path.push_back(p_off);
                        }
                    }
                    if (clean_path.size() < 2) continue;
                    for (size_t k = 0; k < clean_path.size(); ++k) {
                        size_t next_k = (k + 1) % clean_path.size();
                        if (CGAL::squared_distance(clean_path[k], clean_path[next_k]) > FT(1e-12)) {
                            segments.push_back(EK::Segment_2(clean_path[k], clean_path[next_k]));
                        }
                    }
                }
                ContourUtils::build_faces_from_segments(segments, geo);
                x_offset += FT(face->glyph->advance.x) * scale;
            }
            FT_Done_Face(face); FT_Done_FreeType(library);
            vfs->write(fulfilling.with_output("$out"), P::make_shape(vfs, geo, {{"type", "surface"}}));
        } catch (const std::exception& e) {
            std::cerr << "[TextOp] Error: " << e.what() << std::endl;
            vfs->write(fulfilling.with_output("$out"), Shape());
        }
    }
    static std::vector<std::string> argument_keys() { return {"text", "font", "size"}; }
    static typename P::json schema() {
        return { {"path", "jot/Text"}, {"arguments", nlohmann::json::array({ {{"name", "text"}, {"type", "jot:string"}}, {{"name", "font"}, {"type", "jot:font"}}, {{"name", "size"}, {"type", "jot:number"}, {"default", 10}} })}, {"outputs", {{"$out", {{"type", "jot:shape"}}}}} };
    }
};

inline void text_init(fs::VFSNode* vfs) {
    Processor::register_op<TextOp<>, std::string, nlohmann::json, double>(vfs, "jot/Text");
}

} // namespace geo
} // namespace jotcad
