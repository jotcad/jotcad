#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <filesystem>
#include "../impl/protocols.h"
#include "../impl/processor.h"
#include "../../fs/cpp/vfs_node.h"

namespace jotcad {
namespace geo {

/**
 * MockVFS: A minimal file-system-backed VFS for testing.
 */
class MockVFS : public fs::VFSNode {
public:
    MockVFS() : fs::VFSNode(create_config()) {
        std::filesystem::create_directories(".vfs_test_storage");
    }

    ~MockVFS() {
        std::filesystem::remove_all(".vfs_test_storage");
    }

    static fs::VFSNode::Config create_config() {
        fs::VFSNode::Config cfg;
        cfg.id = "mock_peer";
        cfg.storage_dir = ".vfs_test_storage";
        return cfg;
    }

    void verify_render(const Shape& s, const std::string& label, const std::string& expected_hash) {
        std::cout << "  [Verification] " << label << " (Hash check skipped in MockVFS)" << std::endl;
    }
};

} // namespace geo
} // namespace jotcad
