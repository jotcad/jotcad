#pragma once
#include "../../fs/cpp/include/vfs_node.h"
#include "../impl/geometry.h"
#include "../impl/shape.h"
#include <iostream>
#include <vector>
#include <cassert>

namespace jotcad {
namespace geo {

// Forward declaration of the global registration function
void register_all_ops();

class MockVFS : public fs::VFSNode {
public:
    MockVFS() : fs::VFSNode({"mock", "1.0", ".vfs_storage_mock"}) {
        jotcad::geo::register_all_ops();
    }

    // Helper for tests to read geometry from a shape
    Geometry read_geo(const std::optional<fs::Selector>& sel) {
        if (!sel.has_value()) return {};
        return read<Geometry>(*sel);
    }
};

} // namespace geo
} // namespace jotcad
