#pragma once
#include "impl/processor.h"
#include "impl/triangle.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> triangle_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
    std::cout << "[Triangle Op] Generating triangle with params: " << params.dump() << std::endl;
    double a = 0, b = 0, c = 0;
    std::string form = params.value("form", "SSS");

    if (form == "SSS") {
        a = params.value("a", 1.0); 
        b = params.value("b", 1.0); 
        c = params.value("c", 1.0);
    } else if (form == "SAS") {
        a = params.value("a", 1.0); 
        b = params.value("b", 1.0);
        double angle_rad = params.value("angle", 60.0) * M_PI / 180.0;
        c = std::sqrt(a*a + b*b - 2*a*b*std::cos(angle_rad));
    } else if (form == "equilateral") {
        a = b = c = params.value("side", 1.0);
    }

    Geometry geo;
    makeTriangle(geo, a, b, c);

    // 1. Write the raw mesh to a content-addressed location (geo/mesh)
    std::string mesh_text = geo.encode_text();
    std::cout << "[Triangle Op] Generated mesh, " << mesh_text.size() << " bytes. Writing to geo/mesh..." << std::endl;
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    // 2. Return the Shape JSON for the requested path
    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "triangle"}}}
    };
    std::string shape_text = shape.dump(2);
    std::cout << "[Triangle Op] Returning Shape JSON for " << path << std::endl;
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void triangle_init() {
    Processor::Operation op;
    op.path = "shape/triangle";
    op.logic = triangle_op;
    op.schema = {
        {"type", "object"},
        {"properties", {
            {"form", {{"type", "string"}, {"enum", {"SSS", "SAS", "equilateral"}}, {"default", "equilateral"}}},
            {"side", {{"type", "number"}, {"default", 10}}},
            {"a", {{"type", "number"}, {"default", 10}}},
            {"b", {{"type", "number"}, {"default", 10}}},
            {"c", {{"type", "number"}, {"default", 10}}},
            {"angle", {{"type", "number"}, {"default", 60}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
