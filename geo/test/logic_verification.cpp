#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <filesystem>
#include "../impl/geometry.h"
#include "../impl/box.h"
#include "../impl/triangle.h"
#include "../impl/hexagon.h"
#include "../impl/matrix.h"
#include "../impl/processor.h"
#include "../impl/protocols.h"
#include "../box_op.h"
#include "../nth_op.h"
#include "../rotate_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;
using namespace jotcad::fs;
using json = nlohmann::json;

void test_throwing_read() {
    std::cout << "Testing Throwing Read..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    VFSNode vfs(config);

    try {
        vfs.read<json>({ "missing/path", {}, {} });
        assert(false && "Should have thrown");
    } catch (const VFSException& e) {
        assert(e.code == 404);
        std::cout << "✅ Threw 404 on missing path" << std::endl;
    }
}

void test_formal_links() {
    std::cout << "Testing Formal Link Resolution..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_test_links";
    std::filesystem::remove_all(config.storage_dir);
    
    VFSNode vfs(config);

    // 1. Write the target data
    json target_data = {{"id", "target"}, {"type", "data"}};
    std::string target_str = target_data.dump();
    std::vector<uint8_t> target_bytes(target_str.begin(), target_str.end());
    vfs.write("actual/data", {{"id", 1}}, target_bytes);

    // 2. Create the Formal Link
    vfs.link("alias/data", {{"id", 1}}, "actual/data", {{"id", 1}});

    // 3. Read the link (should recursively resolve)
    auto resolved = vfs.read<json>({ "alias/data", {{"id", 1}}, {} });
    
    assert(resolved["id"] == "target");
    std::cout << "✅ Formal Link Resolved Successfully" << std::endl;
    
    std::filesystem::remove_all(config.storage_dir);
}

void test_vectorized_expansion() {
    std::cout << "Testing Vectorized Cartesian Expansion (Box)..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    VFSNode vfs(config);

    // Multiple Widths -> Group of Shapes
    json params = {{"width", {10.0, 20.0}}, {"height", 10.0}};
    auto data = BoxOp<>::logic(&vfs, "jot/Box", params, {});
    json shape = json::parse(data);

    // In the new architecture, constructors return a SHAPE.
    // If plural, the shape's geometry is op/group.
    assert(shape["geometry"]["path"] == "op/group");
    auto items = shape["geometry"]["parameters"]["items"];
    assert(items.size() == 2);
    
    std::cout << "✅ Vectorized Box Expansion PASS" << std::endl;
}

void test_nth_resolution() {
    std::cout << "Testing Nth Resolution & Selection..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_test_nth";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    // A Group Shape
    Shape group_shape;
    group_shape.geometry = {"op/group", json::object()};
    
    Shape s0; s0.tags = {{"id", "zero"}};
    Shape s1; s1.tags = {{"id", "one"}};
    group_shape.geometry.parameters["items"] = {s0.to_json(), s1.to_json()};

    std::string group_str = group_shape.to_json().dump();
    std::vector<uint8_t> group_bytes(group_str.begin(), group_str.end());
    vfs.write("op/mock_group", {}, group_bytes);

    // Selector for the group
    json selector = {{"path", "op/mock_group"}, {"parameters", {}}};
    json params = {{"$in", selector}, {"indices", {1}}};

    auto data = NthOp<>::logic(&vfs, "jot/nth", params, {});
    json result = json::parse(data);

    assert(result["tags"]["id"] == "one");
    std::cout << "✅ Nth Resolved and Selected Index 1 PASS" << std::endl;

    std::filesystem::remove_all(config.storage_dir);
}

int main() {
    try {
        test_throwing_read();
        test_formal_links();
        test_vectorized_expansion();
        test_nth_resolution();
        std::cout << "\nALL LOGIC TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
