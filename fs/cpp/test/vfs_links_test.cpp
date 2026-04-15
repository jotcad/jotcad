#include "../include/vfs_node.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace jotcad::fs;

void test_formal_link() {
    VFSNode::Config config;
    config.id = "test-node-formal";
    config.storage_dir = "./test_storage_formal_links";
    fs::remove_all(config.storage_dir);
    fs::create_directories(config.storage_dir);

    VFSNode node(config);

    // 1. Setup target data
    std::string target_path = "geo/mesh";
    json target_params = {{"cid", "abc123"}};
    std::vector<uint8_t> target_data = {'M', 'E', 'S', 'H'};
    node.write(target_path, target_params, target_data);

    // 2. Create FORMAL link in VFS
    // This writes .meta with "target" field
    std::string src_path = "jot/Triangle/geometry";
    json src_params = {{"side", 10}};
    node.link(src_path, src_params, target_path, target_params);

    // 3. Resolve Link
    VFSNode::VFSRequest req;
    req.path = src_path;
    req.parameters = src_params;
    auto res = node.read(req);
    
    assert(res == target_data);
    std::cout << "✔ C++ Formal Link (Metadata Target) Resolved Successfully" << std::endl;

    fs::remove_all(config.storage_dir);
}

void test_no_guessing() {
    VFSNode::Config config;
    config.id = "test-node-no-guessing";
    config.storage_dir = "./test_storage_no_guessing";
    fs::remove_all(config.storage_dir);
    fs::create_directories(config.storage_dir);

    VFSNode node(config);

    // 1. Setup raw data that LOOKS like a link pointer
    std::string src_path = "fake/link";
    std::string link_ptr = "vfs:/should/not/be/followed";
    std::vector<uint8_t> link_ptr_data(link_ptr.begin(), link_ptr.end());
    node.write(src_path, {}, link_ptr_data);

    // 2. Reading should return the raw data, NOT attempt to follow it
    VFSNode::VFSRequest req;
    req.path = src_path;
    req.parameters = {};
    auto res = node.read(req);
    
    assert(res == link_ptr_data);
    std::cout << "✔ C++ No Guessing: Raw 'vfs:/' strings are not followed" << std::endl;

    fs::remove_all(config.storage_dir);
}

int main() {
    try {
        test_formal_link();
        test_no_guessing();
        std::cout << "All C++ VFS Link tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
