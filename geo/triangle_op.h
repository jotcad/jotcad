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
    static std::vector<std::string> argument_keys() { return {"a", "b", "c"}; }
    static typename P::json schema() {
        return {{"arguments", {{"a", {{"type", "jot:numbers"}, {"default", 10}}}, {"b", {{"type", "jot:numbers"}, {"default", 10}}}, {"c", {{"type", "jot:numbers"}, {"default", 10}}}, {"$out", {{"type", "jot:shape"}}}}}, {"inputs", {}}, {"outputs", {{"$out", {{"type", "shape"}}}}}};
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
    static std::vector<std::string> argument_keys() { return {"a", "angle", "b"}; }
    static typename P::json schema() {
        return {{"arguments", {{"a", {{"type", "jot:numbers"}, {"default", 10}}}, {"angle", {{"type", "jot:numbers"}, {"default", 60}}}, {"b", {{"type", "jot:numbers"}, {"default", 10}}}, {"$out", {{"type", "jot:shape"}}}}}, {"inputs", {}}, {"outputs", {{"$out", {{"type", "shape"}}}}}};
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
    static std::vector<std::string> argument_keys() { return {"diameter"}; }
    static typename P::json schema() {
        return {{"metadata", {{"alias", "jot/Triangle"}}}, {"arguments", {{"diameter", {{"type", "jot:numbers"}, {"default", 10}}}, {"$out", {{"type", "jot:shape"}}}}}, {"inputs", {}}, {"outputs", {{"$out", {{"type", "shape"}}}}}};
    }
};

static void triangle_init() {
    Processor::register_op<EquilateralTriangleOp<>, std::vector<double>>("jot/Triangle/equilateral");
    Processor::register_op<TriangleSSSOp<>, std::vector<double>, std::vector<double>, std::vector<double>>("jot/Triangle/sss");
    Processor::register_op<TriangleSASOp<>, std::vector<double>, std::vector<double>, std::vector<double>>("jot/Triangle/sas");
}

} // namespace geo
} // namespace jotcad
