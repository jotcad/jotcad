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
            
            // Center roughly
            double cosA = (va*va + vc*vc - vb*vb) / (2 * va * vc);
            double sinA = std::sqrt(std::max(0.0, 1.0 - cosA*cosA));
            double cx = (va + vc*cosA) / 3.0;
            double cy = (vc*sinA) / 3.0;
            for (auto& v : geo.vertices) { v.x -= cx; v.y -= cy; }

            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"a",va},{"b",vb},{"c",vc}}, geo, {{"type","triangle"},{"plane","Z0"}}))));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const nlohmann::json& params, const std::vector<std::string>& stack) {
        auto a = Processor::decode<std::vector<double>>(vfs, "a", params, schema(), stack);
        auto b = Processor::decode<std::vector<double>>(vfs, "b", params, schema(), stack);
        auto c = Processor::decode<std::vector<double>>(vfs, "c", params, schema(), stack);
        Shape out;
        execute(vfs, a, b, c, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"a", {{"type", "jot:numbers"}, {"default", 10}}},
                {"b", {{"type", "jot:numbers"}, {"default", 10}}},
                {"c", {{"type", "jot:numbers"}, {"default", 10}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
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
            double vb = b.size() > i ? b[i] : b[0];
            
            double angle_rad = vang * M_PI / 180.0;
            double vc = std::sqrt(va*va + vb*vb - 2*va*vb*std::cos(angle_rad));
            
            Geometry geo; makeTriangle(geo, va, vb, vc);
            
            // Center roughly
            double cosA = (va*va + vc*vc - vb*vb) / (2 * va * vc);
            double sinA = std::sqrt(std::max(0.0, 1.0 - cosA*cosA));
            double cx = (va + vc*cosA) / 3.0;
            double cy = (vc*sinA) / 3.0;
            for (auto& v : geo.vertices) { v.x -= cx; v.y -= cy; }

            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"a",va},{"angle",vang},{"b",vb}}, geo, {{"type","triangle"},{"plane","Z0"}}))));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const nlohmann::json& params, const std::vector<std::string>& stack) {
        auto a = Processor::decode<std::vector<double>>(vfs, "a", params, schema(), stack);
        auto angle = Processor::decode<std::vector<double>>(vfs, "angle", params, schema(), stack);
        auto b = Processor::decode<std::vector<double>>(vfs, "b", params, schema(), stack);
        Shape out;
        execute(vfs, a, angle, b, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"a", {{"type", "jot:numbers"}, {"default", 10}}},
                {"angle", {{"type", "jot:numbers"}, {"default", 60}}},
                {"b", {{"type", "jot:numbers"}, {"default", 10}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EquilateralTriangleOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& diameter, Shape& out) {
        std::vector<Shape> items;
        for (double d : diameter) {
            Geometry geo; makeTriangle(geo, d, d, d);
            double h = (d * 0.86602540378);
            for (auto& v : geo.vertices) { v.x -= d / 2.0; v.y -= h / 3.0; }
            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"d",d}}, geo, {{"type","triangle"},{"plane","Z0"}}))));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const nlohmann::json& params, const std::vector<std::string>& stack) {
        auto d = Processor::decode<std::vector<double>>(vfs, "diameter", params, schema(), stack);
        Shape out;
        execute(vfs, d, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"metadata", {{"alias", "jot/Triangle"}}},
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", 10}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void triangle_init() {
    Processor::register_op("jot/Triangle/equilateral", [](jotcad::fs::VFSNode* vfs, const std::string& p, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return EquilateralTriangleOp<>::logic(vfs, params, stack);
    }, EquilateralTriangleOp<>::schema());

    Processor::register_op("jot/Triangle/sss", [](jotcad::fs::VFSNode* vfs, const std::string& p, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return TriangleSSSOp<>::logic(vfs, params, stack);
    }, TriangleSSSOp<>::schema());

    Processor::register_op("jot/Triangle/sas", [](jotcad::fs::VFSNode* vfs, const std::string& p, const nlohmann::json& params, const std::vector<std::string>& stack) {
        return TriangleSASOp<>::logic(vfs, params, stack);
    }, TriangleSASOp<>::schema());
}

} // namespace geo
} // namespace jotcad
