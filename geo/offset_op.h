#pragma once
#include "impl/processor.h"
#include "impl/offset.h"
#include <iostream>

namespace jotcad {
namespace geo {

static void offset_init() {
    Processor::Operation op;
    op.path = "jot/offset";
    op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Offset Op] Offsetting mesh..." << std::endl;
        double radius = params.value("diameter", 2.0) / 2.0;
        if (params.contains("radius")) radius = params.at("radius").get<double>();
        
        auto in_selector = params.at("$in");
        jotcad::fs::VFSNode::VFSRequest in_req;
        in_req.path = in_selector["path"];
        in_req.parameters = in_selector.value("parameters", nlohmann::json::object());
        in_req.stack = stack;
        auto shape_bytes = vfs->read(in_req);
        if (shape_bytes.empty()) return std::vector<uint8_t>();

        // 1. Parse Input Shape
        nlohmann::json in_shape = nlohmann::json::parse(std::string(shape_bytes.begin(), shape_bytes.end()));
        
        // 2. Resolve Underlying Geometry
        auto geo_selector = in_shape.at("geometry");
        jotcad::fs::VFSNode::VFSRequest geo_req;
        geo_req.path = geo_selector["path"];
        geo_req.parameters = geo_selector.value("parameters", nlohmann::json::object());
        geo_req.stack = stack;
        auto geo_bytes = vfs->read(geo_req);
        if (geo_bytes.empty()) return std::vector<uint8_t>();

        Geometry mesh;
        mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));

        if (in_shape.contains("tf")) {
            mesh.apply_tf(in_shape.at("tf").get<std::vector<double>>());
        }

        applyOffset(mesh, radius);

        // 4. Write result mesh
        std::string mesh_text = mesh.encode_text();
        std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
        std::string hash = vfs->write_cid("geo/mesh", mesh_data);

        // 2. Return the Shape JSON referencing the CID via structured selector
        nlohmann::json shape = {
            {"geometry", {
                {"path", "geo/mesh"},
                {"parameters", {{"cid", hash}}}
            }},
            {"parameters", params},
            {"tags", {{"type", "offset"}}}
        };
        std::string shape_text = shape.dump();
        return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}, {"description", "Input geometry"}}},
            {"$out", {{"type", "shape"}, {"description", "Resulting geometry"}}},
            {"radius", {{"type", "number"}, {"default", 1}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
