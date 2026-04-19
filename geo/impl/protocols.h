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

    static std::vector<uint8_t> write_shape_obj(const Shape& s) {
        std::string out = s.to_json().dump();
        return std::vector<uint8_t>(out.begin(), out.end());
    }

    static Geometry read_shape_geo(fs::VFSNode* vfs, const Shape& s) {
        return vfs->read<Geometry>(s.geometry.value());
    }

    static Shape make_shape(fs::VFSNode* vfs, const Geometry& geo, const json& tags) {
        Shape s;
        // Anonymous materialization (allowed ONLY for Geometry)
        s.geometry = vfs->write<Geometry>(geo);
        s.tags = tags;
        return s;
    }
};

} // namespace geo
} // namespace jotcad
