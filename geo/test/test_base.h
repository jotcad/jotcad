#pragma once
#include "../../fs/cpp/vfs_node.h"
#include "../impl/geometry.h"
#include "../impl/shape.h"
#include "../impl/rasterizer.h"
#include "../../fs/cpp/cid.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cassert>

namespace jotcad {
namespace geo {

// Forward declaration of the global registration function
void register_all_ops();

struct EnvCleaner {
    EnvCleaner() {
        std::filesystem::remove_all(".vfs_storage_mock");
    }
};

class MockVFS : private EnvCleaner, public fs::VFSNode {
public:
    MockVFS() : fs::VFSNode({"mock", "1.0", ".vfs_storage_mock"}) {
        jotcad::geo::register_all_ops();
    }

    // Helper for tests to read geometry from a shape
    Geometry read_geo(const std::optional<fs::CID>& cid) {
        if (!cid.has_value()) return {};
        return read<Geometry>(*cid);
    }

    // New helper to verify rendering output
    void verify_render(const Shape& shape, const std::string& name, const std::string& expected_hash = "") {
        if (!shape.geometry.has_value()) {
            throw std::runtime_error("Shape has no geometry for rendering: " + name);
        }

        Geometry geo = read_geo(shape.geometry);
        std::vector<uint8_t> png_bytes = Rasterizer::render_png(geo, 256, 256);
        std::string actual_hash = fs::vfs_hash256(png_bytes);

        // 1. Save to 'actual/' for local comparison
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/" + name + ".png", std::ios::binary);
        out.write((char*)png_bytes.data(), png_bytes.size());
        out.close();

        // 2. Check if we match the documentation baseline (in geo/ root)
        // Tests run in geo/test/bin/, so baseline is in ../../
        std::string baseline_path = "../../" + name + ".png";
        
        if (expected_hash != "" && actual_hash != expected_hash) {
            std::cerr << "❌ RENDER FAIL: " << name << std::endl;
            std::cerr << "   Expected: " << expected_hash << std::endl;
            std::cerr << "   Actual:   " << actual_hash << std::endl;
            if (!std::filesystem::exists(baseline_path)) {
                std::cerr << "   (Documentation baseline missing at: " << baseline_path << ")" << std::endl;
            }
            exit(1);
        }
        
        if (expected_hash == "") {
            std::cout << "  📸 RENDER (New/Unverified): " << name << " -> " << actual_hash << std::endl;
        } else {
            std::cout << "  ✅ RENDER PASS: " << name << " (" << actual_hash.substr(0, 8) << ")" << std::endl;
        }
    }
};

} // namespace geo
} // namespace jotcad
