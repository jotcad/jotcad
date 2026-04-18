#pragma once
#include <iostream>
#include <cassert>
#include <vector>
#include <filesystem>
#include "../impl/geometry.h"
#include "../impl/shape.h"
#include "../impl/processor.h"
#include "../impl/protocols.h"
#include "../../fs/cpp/include/vfs_node.h"

namespace jotcad {
namespace geo {

using namespace jotcad::fs;

struct MockVFS : public VFSNode {
    MockVFS() : VFSNode({"mock", "0.0.1", ".vfs_storage_mock", {}, 0}) {
        std::filesystem::create_directories(".vfs_storage_mock");
    }
    ~MockVFS() {
        std::filesystem::remove_all(".vfs_storage_mock");
    }

    // Helper to resolve geometry
    Geometry read_geo(const Selector& s) {
        VFSNode::VFSRequest req;
        req.path = s.path;
        req.parameters = s.parameters;
        return read<Geometry>(req);
    }
};

} // namespace geo
} // namespace jotcad
