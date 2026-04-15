#pragma once
#include "impl/processor.h"
#include "impl/triangle.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> triangle_op_internal(jotcad::fs::VFSNode* vfs, double a, double b, double c, const nlohmann::json& params) {
    Geometry geo;
    makeTriangle(geo, a, b, c);

    // 1. Write raw mesh to content-addressed store
    std::string mesh_text = geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    std::string hash = vfs->write_cid("geo/mesh", mesh_data);

    // 2. Return Shape JSON referencing the CID via structured selector
    nlohmann::json shape = {
        {"geometry", {
            {"path", "geo/mesh"},
            {"parameters", {{"cid", hash}}}
        }},
        {"parameters", params},
        {"tags", {{"type", "triangle"}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void triangle_init() {
    // 1. SSS
    {
        Processor::Operation op;
        op.path = "jot/Triangle/sss";
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double a = params.at("a").get<double>();
            double b = params.at("b").get<double>();
            double c = params.at("c").get<double>();
            return triangle_op_internal(vfs, a, b, c, params);
        };
        op.schema = {
            {"arguments", {
                {"a", {{"type", "number"}}},
                {"b", {{"type", "number"}}},
                {"c", {{"type", "number"}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
            }}
        };
        Processor::register_op(op);
    }

    // 2. SAS
    {
        Processor::Operation op;
        op.path = "jot/Triangle/sas";
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double a = params.at("a").get<double>();
            double b = params.at("b").get<double>();
            double angle_rad = params.at("angle").get<double>() * M_PI / 180.0;
            double c = std::sqrt(a*a + b*b - 2*a*b*std::cos(angle_rad));
            return triangle_op_internal(vfs, a, b, c, params);
        };
        op.schema = {
            {"arguments", {
                {"a", {{"type", "number"}}},
                {"angle", {{"type", "number"}}},
                {"b", {{"type", "number"}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
            }}
        };
        Processor::register_op(op);
    }

    // 3. Equilateral
    {
        Processor::Operation op;
        op.path = "jot/Triangle/equilateral";
        op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double side = 0;
            if (params.contains("diameter")) {
                // For equilateral triangle, diameter (circumcircle) d = side / sin(60) => side = d * sin(60)
                side = params.at("diameter").get<double>() * std::sin(M_PI / 3.0);
            } else if (params.contains("side")) {
                side = params.at("side").get<double>();
            }
            return triangle_op_internal(vfs, side, side, side, params);
        };
        op.schema = {
            {"arguments", {
                {"diameter", {{"type", "number"}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
            }}
        };
        Processor::register_op(op);
    }
}

} // namespace geo
} // namespace jotcad
