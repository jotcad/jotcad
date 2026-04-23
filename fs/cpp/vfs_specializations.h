#pragma once

namespace jotcad {

namespace geo {
struct Geometry;
struct Shape;
}

namespace fs {

struct VFSRequest;

// 1. Declare specializations early to prevent premature instantiation
template <typename T> T vfs_read_implementation(class VFSNode* vfs, const VFSRequest& req);

} // namespace fs
} // namespace jotcad
