#include "png_op.h"
#include "rasterizer.h"
#include <iostream>

namespace jotcad {
namespace geo {

void PngOpImpl::execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in_shape) {
    try {
        double ax = fulfilling.parameters.value("ax", 0.0);
        double ay = fulfilling.parameters.value("ay", 0.0);
        int width = fulfilling.parameters.value("width", 256);
        int height = fulfilling.parameters.value("height", 256);

        // Directly delegate to Rasterizer with the root shape
        std::vector<uint8_t> bytes = Rasterizer::render_png(vfs, in_shape, width, height, ax, ay);
        
        if (!bytes.empty()) {
            vfs->write(fulfilling.with_output("$out"), bytes);
        } else {
            vfs->write(fulfilling.with_output("$out"), std::vector<uint8_t>());
        }
    } catch (const std::exception& e) {
        std::cerr << "[PngOp] Error: " << e.what() << std::endl;
    }
}

} // namespace geo
} // namespace jotcad
