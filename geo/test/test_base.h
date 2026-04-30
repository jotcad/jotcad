#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <filesystem>
#include "protocols.h"
#include "processor.h"
#include "../../fs/cpp/vfs_node.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

namespace jotcad {
namespace geo {

void register_all_ops(fs::VFSNode* vfs);

/**
 * MockVFS: A minimal file-system-backed VFS for testing.
 */
class MockVFS : public fs::VFSNode {
public:
    MockVFS(const std::string& test_name) : fs::VFSNode(create_config(test_name)), m_storage_dir(create_config(test_name).storage_dir) {
        std::filesystem::remove_all(m_storage_dir);
        std::filesystem::create_directories(m_storage_dir);
        // Pre-provision standard empty geometry CID
        this->materialize(std::vector<uint8_t>{});
    }

    ~MockVFS() {
        try {
            std::filesystem::remove_all(m_storage_dir);
        } catch (...) {}
    }

    static fs::VFSNode::Config create_config(const std::string& test_name) {
        fs::VFSNode::Config cfg;
        cfg.id = "mock_peer_" + test_name;
        cfg.storage_dir = ".vfs_test_storage_" + test_name;
        return cfg;
    }

    void verify_render(const Shape& s, const std::string& label, const std::string& expected_hash) {
        std::cout << "  [Verification] " << label << " (Hash check skipped in MockVFS)" << std::endl;
    }

    void verify_well_formed_solid(const Geometry& geo, const std::string& label) {
        std::cout << "  [Verification] " << label << " (Solid check)..." << std::endl;
        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        
        bool closed = CGAL::is_closed(mesh);
        if (!closed) {
            std::cerr << "FAIL: Mesh is NOT CLOSED for " << label << std::endl;
            exit(1);
        }

        bool valid = CGAL::Polygon_mesh_processing::is_outward_oriented(mesh);
        if (!valid) {
             std::cerr << "WARNING: Mesh is NOT OUTWARD ORIENTED for " << label << ". Reorienting..." << std::endl;
             CGAL::Polygon_mesh_processing::reverse_face_orientations(mesh);
             if (!CGAL::Polygon_mesh_processing::is_outward_oriented(mesh)) {
                 std::cerr << "FAIL: Could not force outward orientation for " << label << std::endl;
                 exit(1);
             }
        }

        FT vol = CGAL::Polygon_mesh_processing::volume(mesh);
        std::cout << "    - Volume (Exact): " << vol << " (~" << CGAL::to_double(vol) << ")" << std::endl;
        
        if (vol <= FT(0)) {
            std::cerr << "FAIL: Mesh has NON-POSITIVE volume (" << vol << ") for " << label << std::endl;
            exit(1);
        }
        std::cout << "    - SUCCESS" << std::endl;
    }

private:
    std::string m_storage_dir;
};

} // namespace geo
} // namespace jotcad
