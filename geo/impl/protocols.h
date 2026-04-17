#pragma once
#include <string>
#include <vector>
#include <json.hpp>
#include "../../fs/cpp/include/vfs_node.h"
#include "geometry.h"
#include "shape.h"

namespace jotcad {
namespace geo {

/**
 * JotVfsProtocol: The universal capability trait for JOT VFS Operators.
 */
struct JotVfsProtocol {
    using json = nlohmann::json;

    /**
     * Marshall a JOT Shape
     */
    static std::vector<uint8_t> write_shape(jotcad::fs::VFSNode* vfs, const json& params, const Geometry& geo, const json& tags) {
        std::string mesh_text = geo.encode_text();
        std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
        std::string hash = vfs->write_cid("geo/mesh", mesh_data);

        Shape s;
        s.geometry = {"geo/mesh", {{"cid", hash}}};
        s.tags = tags;
        // The tf is identity by default in Shape struct

        std::string out = s.to_json().dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    static std::vector<uint8_t> write_group(const std::vector<Shape>& items) {
        Shape group_shape;
        group_shape.geometry = {"op/group", json::object()};
        
        json items_json = json::array();
        for (const auto& item : items) items_json.push_back(item.to_json());
        group_shape.geometry.parameters["items"] = items_json;

        std::string out = group_shape.to_json().dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    static std::vector<uint8_t> write_json(const json& j) {
        std::string out = j.dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }
    
    static std::vector<uint8_t> write_shape_obj(const Shape& s) {
        std::string out = s.to_json().dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }
};

} // namespace geo
} // namespace jotcad
