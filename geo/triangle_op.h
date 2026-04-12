#pragma once
#include "impl/processor.h"
#include "impl/triangle.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> triangle_op_internal(jotcad::fs::VFSClient* vfs, double a, double b, double c, const nlohmann::json& params) {
    Geometry geo;
    makeTriangle(geo, a, b, c);
    std::string mesh_text = geo.encode_text();
    return std::vector<uint8_t>(mesh_text.begin(), mesh_text.end());
}

static void triangle_init() {
    // 1. SSS
    {
        Processor::Operation op;
        op.path = "shape/triangle/sss";
        op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double a = params.at("a").get<double>();
            double b = params.at("b").get<double>();
            double c = params.at("c").get<double>();
            return triangle_op_internal(vfs, a, b, c, params);
        };
        op.schema = {
            {"type", "object"},
            {"required", {"a", "b", "c"}},
            {"properties", {
                {"a", {{"type", "number"}}},
                {"b", {{"type", "number"}}},
                {"c", {{"type", "number"}}}
            }}
        };
        Processor::register_op(op);
    }

    // 2. SAS
    {
        Processor::Operation op;
        op.path = "shape/triangle/sas";
        op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double a = params.at("a").get<double>();
            double b = params.at("b").get<double>();
            double angle_rad = params.at("angle").get<double>() * M_PI / 180.0;
            double c = std::sqrt(a*a + b*b - 2*a*b*std::cos(angle_rad));
            return triangle_op_internal(vfs, a, b, c, params);
        };
        op.schema = {
            {"type", "object"},
            {"required", {"a", "angle", "b"}},
            {"properties", {
                {"a", {{"type", "number"}}},
                {"angle", {{"type", "number"}}},
                {"b", {{"type", "number"}}}
            }}
        };
        Processor::register_op(op);
    }

    // 3. Equilateral
    {
        Processor::Operation op;
        op.path = "shape/triangle/equilateral";
        op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double side = params.at("side").get<double>();
            return triangle_op_internal(vfs, side, side, side, params);
        };
        op.schema = {
            {"type", "object"},
            {"required", {"side"}},
            {"properties", {
                {"side", {{"type", "number"}}}
            }}
        };
        Processor::register_op(op);
    }
}

} // namespace geo
} // namespace jotcad
