#pragma once

namespace jotcad {

namespace geo {
struct Geometry;
struct Shape;
}

namespace fs {
struct VFSRequest;
class VFSNode;

// 1. Declare specializations early to satisfy VFSNode::read usage
template <typename T> T vfs_read_implementation(VFSNode* vfs, const VFSRequest& req);

// 2. Explicitly declare specializations for geometry types
template <> jotcad::geo::Geometry vfs_read_implementation<jotcad::geo::Geometry>(VFSNode* vfs, const VFSRequest& req);
template <> jotcad::geo::Shape vfs_read_implementation<jotcad::geo::Shape>(VFSNode* vfs, const VFSRequest& req);

} // namespace fs
} // namespace jotcad
