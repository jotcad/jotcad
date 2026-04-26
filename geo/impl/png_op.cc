#include "png_op.h"
#include "rasterizer.h"
#include "matrix.h"
#include <iostream>

namespace jotcad {
namespace geo {

void PngOpImpl::execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in_shape) {
    try {
        Geometry geo;
        bool has_data = false;

        auto collect_geo = [&](auto self, const Shape& s, const Matrix& tf) -> void {
            Matrix current_tf = tf * Matrix::from_vec(s.tf);
            if (s.geometry.has_value()) {
                Geometry child_geo = vfs->read<Geometry>(s.geometry.value());
                for (const auto& v : child_geo.vertices) {
                    Point_3 src_p(v.x, v.y, v.z);
                    Point_3 dest_p = current_tf.t.transform(src_p);
                    geo.vertices.push_back({dest_p.x(), dest_p.y(), dest_p.z()});
                }
                has_data = true;
            }
            for (const auto& child : s.components) {
                self(self, child, current_tf);
            }
        };

        collect_geo(collect_geo, in_shape, Matrix::identity());

        if (has_data) {
            double ax = fulfilling.parameters.value("ax", 0.0);
            double ay = fulfilling.parameters.value("ay", 0.0);
            std::vector<uint8_t> bytes = Rasterizer::render_png(geo, 256, 256, ax, ay);
            if (!bytes.empty()) {
                vfs->write(fulfilling.with_output("file"), bytes);
            }
        } else {
            std::cerr << "[PngOp] WARNING: Input shape has no geometry to render." << std::endl;
            vfs->write(fulfilling.with_output("file"), std::vector<uint8_t>());
        }
    } catch (const std::exception& e) {
        std::cerr << "[PngOp] Error: " << e.what() << std::endl;
    }
}

} // namespace geo
} // namespace jotcad
