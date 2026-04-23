#pragma once
#include <string>
#include "shape.h"
#include "vfs_node.h"

namespace jotcad {
namespace geo {

class PngOpImpl {
public:
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in_shape);
};

void png_init();

} // namespace geo
} // namespace jotcad
