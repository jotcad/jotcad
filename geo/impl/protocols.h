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
     * Serialize a Shape to VFS JSON bytes
     */
    static std::vector<uint8_t> write_shape_obj(const Shape& s) {
        std::string out = s.to_json().dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    /**
     * Assemble a JOT Shape from geometry and tags
     */
    static Shape make_shape(jotcad::fs::VFSNode* vfs, const Geometry& geo, const json& tags) {
        Shape s;
        s.geometry = vfs->write_geometry(geo);
        s.tags = tags;
        return s;
    }

    static std::vector<uint8_t> write_group(const std::vector<Shape>& items) {
        Shape group_shape;
        // Group has no geometry of its own
        group_shape.geometry = std::nullopt;
        
        json items_json = json::array();
        for (const auto& item : items) items_json.push_back(item.to_json());
        group_shape.add_tag("items", items_json); // Standard grouping tag

        return write_shape_obj(group_shape);
    }

    static std::vector<uint8_t> write_json(const json& j) {
        std::string out = j.dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    static std::vector<uint8_t> write_json(const Shape& s) {
        return write_shape_obj(s);
    }
};

} // namespace geo
} // namespace jotcad
