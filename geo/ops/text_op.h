#pragma once
#include "../core/protocols.h"
#include "../core/processor.h"
#include "../render/triangulation.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <vector>
#include <string>
#include <iostream>
#include <CGAL/Polygon_2.h>

namespace jotcad {
namespace geo {

typedef CGAL::Polygon_2<EK> Polygon;

struct OutlineContext {
    std::vector<std::vector<Vertex>> paths;
    Vertex current_pos;
    double scale;

    static int move_to(const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        ctx->paths.push_back({});
        ctx->current_pos = { (FT)to->x * ctx->scale, (FT)to->y * ctx->scale, 0 };
        ctx->paths.back().push_back(ctx->current_pos);
        return 0;
    }

    static int line_to(const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        ctx->current_pos = { (FT)to->x * ctx->scale, (FT)to->y * ctx->scale, 0 };
        ctx->paths.back().push_back(ctx->current_pos);
        return 0;
    }

    static int conic_to(const FT_Vector* control, const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        Vertex p0 = ctx->current_pos;
        Vertex p1 = { (FT)control->x * ctx->scale, (FT)control->y * ctx->scale, 0 };
        Vertex p2 = { (FT)to->x * ctx->scale, (FT)to->y * ctx->scale, 0 };
        for (int i = 1; i <= 8; ++i) {
            double t = i / 8.0;
            double invT = 1.0 - t;
            ctx->current_pos = {
                (FT)(invT * invT * p0.x + 2 * invT * t * p1.x + t * t * p2.x),
                (FT)(invT * invT * p0.y + 2 * invT * t * p1.y + t * t * p2.y),
                0
            };
            ctx->paths.back().push_back(ctx->current_pos);
        }
        return 0;
    }

    static int cubic_to(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) {
        auto ctx = static_cast<OutlineContext*>(user);
        Vertex p0 = ctx->current_pos;
        Vertex p1 = { (FT)control1->x * ctx->scale, (FT)control1->y * ctx->scale, 0 };
        Vertex p2 = { (FT)control2->x * ctx->scale, (FT)control2->y * ctx->scale, 0 };
        Vertex p3 = { (FT)to->x * ctx->scale, (FT)to->y * ctx->scale, 0 };
        for (int i = 1; i <= 8; ++i) {
            double t = i / 8.0;
            double invT = 1.0 - t;
            ctx->current_pos = {
                (FT)(invT * invT * invT * p0.x + 3 * invT * invT * t * p1.x + 3 * invT * t * t * p2.x + t * t * t * p3.x),
                (FT)(invT * invT * invT * p0.y + 3 * invT * invT * t * p1.y + 3 * invT * t * t * p2.y + t * t * t * p3.y),
                0
            };
            ctx->paths.back().push_back(ctx->current_pos);
        }
        return 0;
    }
};

template <typename P = JotVfsProtocol>
struct TextOp : P {
    static constexpr const char* path = "jot/text";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::string& text, const nlohmann::json& font_arg, double size) {
        try {
            std::vector<uint8_t> font_data;
            if (font_arg.is_string()) {
                std::string s = font_arg.get<std::string>();
                if (s.find("://") != std::string::npos) {
                    font_data = vfs->read<std::vector<uint8_t>>(fs::Selector("jot/Font", {{"url", s}}).with_output("$out"));
                } else if (s.size() == 64) {
                    font_data = vfs->read<std::vector<uint8_t>>(fs::CID{s});
                } else {
                    throw std::runtime_error("Invalid font string (neither URL nor CID)");
                }
            } else if (font_arg.is_object() && font_arg.contains("path")) {
                fs::Selector font_sel = font_arg.get<fs::Selector>();
                if (font_sel.output.empty()) font_sel = font_sel.with_output("$out");
                font_data = vfs->read<std::vector<uint8_t>>(font_sel);
            } else {
                throw std::runtime_error("Invalid font identity type");
            }

            if (font_data.empty()) throw std::runtime_error("Font data empty or download failed");

            FT_Library library;
            if (FT_Init_FreeType(&library)) throw std::runtime_error("FT_Init_FreeType failed");

            FT_Face face;
            if (FT_New_Memory_Face(library, font_data.data(), (FT_Long)font_data.size(), 0, &face)) {
                FT_Done_FreeType(library);
                throw std::runtime_error("FT_New_Memory_Face failed");
            }

            FT_Set_Pixel_Sizes(face, 0, 64);
            Geometry geo;
            double x_offset = 0;
            double scale = size / 64.0 / 64.0;

            FT_Outline_Funcs funcs;
            funcs.move_to = OutlineContext::move_to;
            funcs.line_to = OutlineContext::line_to;
            funcs.conic_to = OutlineContext::conic_to;
            funcs.cubic_to = OutlineContext::cubic_to;
            funcs.shift = 0; funcs.delta = 0;

            for (char c : text) {
                FT_Error err = FT_Load_Char(face, c, FT_LOAD_NO_BITMAP);
                if (err) {
                    std::cerr << "[TextOp] FT_Load_Char failed for '" << c << "' Error: " << err << std::endl;
                    continue;
                }

                OutlineContext ctx;
                ctx.scale = scale;
                ctx.current_pos = {0, 0, 0};

                err = FT_Outline_Decompose(&face->glyph->outline, &funcs, &ctx);
                if (err) {
                    std::cerr << "[TextOp] FT_Outline_Decompose failed for '" << c << "' Error: " << err << std::endl;
                    continue;
                }

                std::cout << "[TextOp] Char '" << c << "' decomposed into " << ctx.paths.size() << " paths." << std::endl;
                
                std::vector<Polygon> polygons;
                for (size_t i = 0; i < ctx.paths.size(); ++i) {
                    auto& raw_path = ctx.paths[i];
                    if (raw_path.size() < 3) continue;

                    // Deduplicate consecutive points
                    std::vector<Vertex> clean_path;
                    for (const auto& v : raw_path) {
                        if (clean_path.empty() || 
                            std::abs(CGAL::to_double(clean_path.back().x) - CGAL::to_double(v.x)) > 1e-9 || 
                            std::abs(CGAL::to_double(clean_path.back().y) - CGAL::to_double(v.y)) > 1e-9) {
                            clean_path.push_back(v);
                        }
                    }
                    // Loop closure check: remove redundant last point if it matches first
                    if (clean_path.size() >= 3 && 
                        std::abs(CGAL::to_double(clean_path.front().x) - CGAL::to_double(clean_path.back().x)) < 1e-9 && 
                        std::abs(CGAL::to_double(clean_path.front().y) - CGAL::to_double(clean_path.back().y)) < 1e-9) {
                        clean_path.pop_back();
                    }

                    if (clean_path.size() < 3) continue;

                    Polygon poly;
                    for (auto& v : clean_path) { 
                        poly.push_back(EK::Point_2(v.x + x_offset, v.y)); 
                    }
                    
                    bool simple = poly.is_simple();
                    std::cout << "  - Path " << i << ": size=" << poly.size() << ", simple=" << (simple ? "YES" : "NO") << std::endl;
                    if (poly.size() >= 3 && simple) {
                        polygons.push_back(poly);
                    } else if (!simple) {
                        std::cout << "    [TextOp] WARNING: Skipping non-simple path for character '" << c << "'" << std::endl;
                    }
                }

                // Hole Detection via Point-in-Polygon Parity
                std::vector<bool> is_hole(polygons.size(), false);
                for (size_t i = 0; i < polygons.size(); ++i) {
                    int parent_count = 0;
                    for (size_t j = 0; j < polygons.size(); ++j) {
                        if (i == j) continue;
                        if (polygons[j].bounded_side(polygons[i][0]) == CGAL::ON_BOUNDED_SIDE) parent_count++;
                    }
                    if (parent_count % 2 != 0) is_hole[i] = true;
                }

                struct FaceGroup { size_t outer; std::vector<size_t> holes; };
                std::vector<FaceGroup> groups;
                for (size_t i = 0; i < polygons.size(); ++i) {
                    if (!is_hole[i]) groups.push_back({i, {}});
                }
                for (size_t i = 0; i < polygons.size(); ++i) {
                    if (is_hole[i]) {
                        int best_parent = -1;
                        for (size_t g = 0; g < groups.size(); ++g) {
                            if (polygons[groups[g].outer].bounded_side(polygons[i][0]) == CGAL::ON_BOUNDED_SIDE) {
                                if (best_parent == -1 || polygons[groups[best_parent].outer].bounded_side(polygons[groups[g].outer][0]) == CGAL::ON_BOUNDED_SIDE) best_parent = g;
                            }
                        }
                        if (best_parent != -1) groups[best_parent].holes.push_back(i);
                    }
                }

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
                x_offset += face->glyph->advance.x * scale;
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
        return {
            {"path", "jot/text"},
            {"description", "Generates 2D geometry from text using a font."},
            {"arguments", {
                {{"name", "text"}, {"type", "jot:string"}},
                {{"name", "font"}, {"type", "jot:font"}},
                {{"name", "size"}, {"type", "jot:number"}, {"default", 10}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void text_init(fs::VFSNode* vfs) {
    Processor::register_op<TextOp<>, std::string, nlohmann::json, double>(vfs, "jot/text");
}

} // namespace geo
} // namespace jotcad
