#include "png_op.h"
#include "rasterizer.h"
#include <iostream>

namespace jotcad {
namespace geo {

void PngOpImpl::execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in_shape) {
    if (in_shape.geometry.has_value()) {
        try {
            Geometry geo = vfs->read<Geometry>(in_shape.geometry.value());
            double ax = fulfilling.parameters.value("ax", 0.0);
            double ay = fulfilling.parameters.value("ay", 0.0);
            std::vector<uint8_t> bytes = Rasterizer::render_png(geo, 256, 256, ax, ay);
            if (!bytes.empty()) {
                vfs->write<std::vector<uint8_t>>(fulfilling, bytes, "file");
            }
        } catch (const std::exception& e) {
            std::cerr << "[PngOp] Error: " << e.what() << std::endl;
        }
    }
}

} // namespace geo
} // namespace jotcad
