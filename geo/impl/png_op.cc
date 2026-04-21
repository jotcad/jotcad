#include "png_op.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../fs/cpp/vendor/stb_image_write.h"
#include "geometry.h"
#include <vector>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {

void PngOpImpl::execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in_shape) {
    int width = 256;
    int height = 256;
    std::vector<unsigned char> pixels(width * height * 3, 240); // Light gray background

    if (in_shape.geometry.has_value()) {
        try {
            Geometry geo = vfs->read<Geometry>(in_shape.geometry.value());
            if (!geo.vertices.empty()) {
                // Find bounding box for normalization
                double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
                for (const auto& v : geo.vertices) {
                    double vx = CGAL::to_double(v.x);
                    double vy = CGAL::to_double(v.y);
                    min_x = std::min(min_x, vx); max_x = std::max(max_x, vx);
                    min_y = std::min(min_y, vy); max_y = std::max(max_y, vy);
                }

                double dx = max_x - min_x;
                double dy = max_y - min_y;
                double scale = 0.8 * std::min(width / (dx > 0 ? dx : 1.0), height / (dy > 0 ? dy : 1.0));
                double offset_x = width / 2.0 - (min_x + max_x) / 2.0 * scale;
                double offset_y = height / 2.0 - (min_y + max_y) / 2.0 * scale;

                // Draw vertices as black pixels
                for (const auto& v : geo.vertices) {
                    int px = (int)(CGAL::to_double(v.x) * scale + offset_x);
                    int py = (int)(CGAL::to_double(v.y) * scale + offset_y);
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        int idx = (py * width + px) * 3;
                        pixels[idx] = 0; pixels[idx+1] = 0; pixels[idx+2] = 0;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PngOp] Error reading geometry: " << e.what() << std::endl;
        }
    }

    int len;
    unsigned char* png_data = stbi_write_png_to_mem(pixels.data(), width * 3, width, height, 3, &len);
    if (png_data) {
        std::vector<unsigned char> bytes(png_data, png_data + len);
        // Write result to the 'file' port of the fulfilling address
        vfs->write<std::vector<uint8_t>>(fulfilling, bytes, "file");
        free(png_data);
    }
}

} // namespace geo
} // namespace jotcad
