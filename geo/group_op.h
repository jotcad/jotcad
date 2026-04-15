#pragma once
#include "impl/processor.h"
#include "impl/geometry.h"
#include <vector>

namespace jotcad {
namespace geo {

static void group_init() {
    Processor::Operation op;
    op.path = "jot/group";
    op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Group Op] Merging shapes..." << std::endl;
        
        auto sources = params.at("$in").get<std::vector<nlohmann::json>>();
        Geometry combined;

        for (const auto& s : sources) {
            jotcad::fs::VFSNode::VFSRequest in_req;
            in_req.path = s["path"];
            in_req.parameters = s.value("parameters", nlohmann::json::object());
            in_req.stack = stack;
            auto shape_bytes = vfs->read(in_req);
            if (shape_bytes.empty()) continue;

            nlohmann::json in_shape = nlohmann::json::parse(std::string(shape_bytes.begin(), shape_bytes.end()));
            
            auto geo_selector = in_shape.at("geometry");
            jotcad::fs::VFSNode::VFSRequest geo_req;
            geo_req.path = geo_selector["path"];
            geo_req.parameters = geo_selector.value("parameters", nlohmann::json::object());
            geo_req.stack = stack;
            auto geo_bytes = vfs->read(geo_req);
            if (geo_bytes.empty()) continue;

            Geometry mesh;
            mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));

            if (in_shape.contains("tf")) {
                mesh.apply_tf(in_shape.at("tf").get<std::vector<double>>());
            }

            // Offset indices by existing vertex count
            int offset = (int)combined.vertices.size();
            combined.vertices.insert(combined.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            for (auto p : mesh.points) combined.points.push_back(p + offset);
            for (auto s : mesh.segments) combined.segments.push_back({s[0] + offset, s[1] + offset});
            for (auto t : mesh.triangles) combined.triangles.push_back({t[0] + offset, t[1] + offset, t[2] + offset});
            for (auto f : mesh.faces) {
                Geometry::Face nf;
                for (auto l : f.loops) {
                    std::vector<int> nl;
                    for (int i : l) nl.push_back(i + offset);
                    nf.loops.push_back(nl);
                }
                combined.faces.push_back(nf);
            }
        }

        std::string mesh_text = combined.encode_text();
        std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
        std::string hash = vfs->write_cid("geo/mesh", mesh_data);

        nlohmann::json shape = {
            {"geometry", {
                {"path", "geo/mesh"},
                {"parameters", {{"cid", hash}}}
            }},
            {"parameters", params},
            {"tags", {{"type", "group"}}}
        };
        std::string res = shape.dump();
        return std::vector<uint8_t>(res.begin(), res.end());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "array"}, {"items", {{"type", "shape"}}}}}
        }},
        {"inputs", {
            {"$in", {{"type", "array"}, {"items", {{"type", "shape"}}}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
