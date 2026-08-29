#include "../vfs_node.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace stdfs = std::filesystem;
using namespace fs;

void test_formal_link() {
    VFSNode::Config config;
    config.id = "test-node-formal";
    config.storage_dir = "./test_storage_formal_links";
    stdfs::remove_all(config.storage_dir);
    stdfs::create_directories(config.storage_dir);

    VFSNode node(config);

    // 1. Setup target data
    Selector target_sel;
    target_sel.path = "geo/mesh";
    target_sel.parameters = {{"cid", "abc123"}};
    std::vector<uint8_t> target_data = {'M', 'E', 'S', 'H'};
    node.write(target_sel, target_data);

    // 2. Create FORMAL link in VFS
    // This writes .meta with "target" field
    Selector src_sel;
    src_sel.path = "jot/Triangle/geometry";
    src_sel.parameters = {{"side", 10}};
    node.link(src_sel, target_sel);

    // 3. Resolve Link
    VFSNode::VFSRequest req;
    req.selector = src_sel;
    auto res = node.read<std::vector<uint8_t>>(req);
    
    assert(res == target_data);
    std::cout << "✔ C++ Formal Link (Metadata Target) Resolved Successfully" << std::endl;

    stdfs::remove_all(config.storage_dir);
}

void test_no_guessing() {
    VFSNode::Config config;
    config.id = "test-node-no-guessing";
    config.storage_dir = "./test_storage_no_guessing";
    stdfs::remove_all(config.storage_dir);
    stdfs::create_directories(config.storage_dir);

    VFSNode node(config);

    // 1. Setup raw data that LOOKS like a link pointer
    Selector src_sel;
    src_sel.path = "fake/link";
    std::string link_ptr = "vfs:/should/not/be/followed";
    std::vector<uint8_t> link_ptr_data(link_ptr.begin(), link_ptr.end());
    node.write(src_sel, link_ptr_data);

    // 2. Reading should return the raw data, NOT attempt to follow it
    VFSNode::VFSRequest req;
    req.selector = src_sel;
    auto res = node.read<std::vector<uint8_t>>(req);
    
    assert(res == link_ptr_data);
    std::cout << "✔ C++ No Guessing: Raw 'vfs:/' strings are not followed" << std::endl;

    stdfs::remove_all(config.storage_dir);
}

void test_missing_cid_throws_404() {
    VFSNode::Config config;
    config.id = "test-node-missing";
    config.storage_dir = "./test_storage_missing";
    stdfs::remove_all(config.storage_dir);
    stdfs::create_directories(config.storage_dir);

    VFSNode node(config);

    CID non_existent_cid{"0000000000000000000000000000000000000000000000000000000000000000"};
    
    bool caught_404 = false;
    try {
        node.read<std::vector<uint8_t>>(non_existent_cid);
    } catch (const VFSException& e) {
        if (e.code == 404) caught_404 = true;
    }
    assert(caught_404);
    std::cout << "✔ VFS Missing CID throws 404 VFSException" << std::endl;

    stdfs::remove_all(config.storage_dir);
}

void test_type_mismatch_throws_400() {
    VFSNode::Config config;
    config.id = "test-node-mismatch";
    config.storage_dir = "./test_storage_mismatch";
    stdfs::remove_all(config.storage_dir);
    stdfs::create_directories(config.storage_dir);

    VFSNode node(config);

    Selector sel;
    sel.path = "test/string_value";
    std::string str_val = "not_a_number";
    std::vector<uint8_t> data(str_val.begin(), str_val.end());
    node.write(sel, data);

    bool caught_400 = false;
    try {
        node.read<double>(sel);
    } catch (const VFSException& e) {
        if (e.code == 400 || e.code == 500) caught_400 = true;
    }
    assert(caught_400);
    std::cout << "✔ VFS Type Mismatch throws 400/500 VFSException" << std::endl;

    stdfs::remove_all(config.storage_dir);
}

int main() {
    try {
        test_formal_link();
        test_no_guessing();
        test_missing_cid_throws_404();
        test_type_mismatch_throws_400();
        std::cout << "All C++ VFS Link and Error Handling tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
