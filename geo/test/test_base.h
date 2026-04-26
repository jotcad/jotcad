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

private:
    std::string m_storage_dir;
};

} // namespace geo
} // namespace jotcad
