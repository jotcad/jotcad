#pragma once
#include "impl/processor.h"
#include "impl/geometry.h"
#include <sstream>
#include <iomanip>
#include <set>

namespace jotcad {
namespace geo {

static void points_init() {
    Processor::Operation op;
    op.path = "jot/points";
    op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Points Op] Extracting points..." << std::endl;
        
        auto in_selector = params.at("$in");
        jotcad::fs::VFSNode::VFSRequest in_req;
        in_req.path = in_selector["path"];
        in_req.parameters = in_selector.value("parameters", nlohmann::json::object());
        in_req.stack = stack;
        auto shape_bytes = vfs->read(in_req);
        if (shape_bytes.empty()) return std::vector<uint8_t>();

        nlohmann::json in_shape = nlohmann::json::parse(std::string(shape_bytes.begin(), shape_bytes.end()));
        
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

        // Logic: Extract all vertices as a point cloud
        Geometry out_geo;
        out_geo.vertices = mesh.vertices;
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            out_geo.points.push_back((int)i);
        }

        std::string mesh_text = out_geo.encode_text();
        std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
        std::string hash = vfs->write_cid("geo/mesh", mesh_data);

        nlohmann::json shape = {
            {"geometry", {
                {"path", "geo/mesh"},
                {"parameters", {{"cid", hash}}}
            }},
            {"parameters", params},
            {"tags", {{"type", "points"}}}
        };
        std::string res = shape.dump();
        return std::vector<uint8_t>(res.begin(), res.end());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}}}
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
