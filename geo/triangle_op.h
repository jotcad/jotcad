#pragma once
#include "impl/protocols.h"
#include "impl/triangle.h"
#include "impl/processor.h"
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TriangleSSSOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c, Shape& out) {
        std::vector<Shape> items;
        for (size_t i = 0; i < a.size(); ++i) {
            double va = a[i];
            double vb = b.size() > i ? b[i] : a[i];
            double vc = c.size() > i ? c[i] : a[i];
            Geometry geo; makeTriangle(geo, va, vb, vc);
            
            // Center the triangle
            FT cx = (FT(va) + geo.vertices[2].x) / FT(3);
            FT cy = geo.vertices[2].y / FT(3);
            for (auto& v : geo.vertices) { v.x -= cx; v.y -= cy; }

            items.push_back(P::make_shape(vfs, geo, {{"type","triangle"},{"plane","Z0"}}));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = std::nullopt;
            out.components = items;
            out.add_tag("type", "group");
        }
    }
    static std::vector<std::string> argument_keys() { return {"a", "b", "c"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle/sss"},
            {"arguments", {
                {"a", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"b", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"c", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TriangleSASOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& a, const std::vector<double>& angle, const std::vector<double>& b, Shape& out) {
        std::vector<Shape> items;
        for (size_t i = 0; i < a.size(); ++i) {
            double va = a[i];
            double vang = angle.size() > i ? angle[i] : angle[0];
            double vb_side = b.size() > i ? b[i] : b[0];
            double angle_rad = vang * M_PI / 180.0;
            double vc = std::sqrt(va*va + vb_side*vb_side - 2*va*vb_side*std::cos(angle_rad));
            Geometry geo; makeTriangle(geo, va, vb_side, vc);
            
            FT cx = (FT(va) + geo.vertices[2].x) / FT(3);
            FT cy = geo.vertices[2].y / FT(3);
            for (auto& v : geo.vertices) { v.x -= cx; v.y -= cy; }

            items.push_back(P::make_shape(vfs, geo, {{"type","triangle"},{"plane","Z0"}}));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = std::nullopt;
            out.components = items;
            out.add_tag("type", "group");
        }
    }
    static std::vector<std::string> argument_keys() { return {"a", "angle", "b"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle/sas"},
            {"arguments", {
                {"a", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"angle", {{"type", "jot:numbers"}, {"default", {60.0}}}},
                {"b", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EquilateralTriangleOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& diameter, Shape& out) {
        std::vector<Shape> items;
        for (double d : diameter) {
            Geometry geo; makeTriangle(geo, d, d, d);
            FT cx = (FT(d) + geo.vertices[2].x) / FT(3);
            FT cy = geo.vertices[2].y / FT(3);
            for (auto& v : geo.vertices) { v.x -= cx; v.y -= cy; }
            items.push_back(P::make_shape(vfs, geo, {{"type","triangle"},{"plane","Z0"}}));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = std::nullopt;
            out.components = items;
            out.add_tag("type", "group");
        }
    }
    static std::vector<std::string> argument_keys() { return {"diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle/equilateral"},
            {"aliases", {"jot/Triangle", "jot/triangle"}},
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void triangle_init() {
    Processor::register_op<EquilateralTriangleOp<>, Shape, std::vector<double>>("jot/Triangle/equilateral");
    Processor::register_op<TriangleSSSOp<>, Shape, std::vector<double>, std::vector<double>, std::vector<double>>("jot/Triangle/sss");
    Processor::register_op<TriangleSASOp<>, Shape, std::vector<double>, std::vector<double>, std::vector<double>>("jot/Triangle/sas");
}

} // namespace geo
} // namespace jotcad
