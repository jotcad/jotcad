#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <filesystem>
#include "vfs_node.h"
#include "processor.h"
#include "box_op.h"
#include "hexagon_op.h"
#include "nth_op.h"
#include "rotate_op.h"
#include "shape.h"

using namespace jotcad::geo;
using namespace fs;
using json = nlohmann::json;

void test_throwing_read() {
    std::cout << "Running test_throwing_read..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_logic_test_throwing";
    std::filesystem::remove_all(config.storage_dir);
    
    VFSNode vfs(config);

    try {
        vfs.read(Selector("missing/path"));
        assert(false && "Should have thrown VFSException");
    } catch (const VFSException& e) {
        assert(e.code == 404);
        std::cout << "✅ Caught expected 404 PASS" << std::endl;
    }
    std::filesystem::remove_all(config.storage_dir);
}

void test_formal_links() {
    std::cout << "Running test_formal_links..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_logic_test_links";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    // Write some "actual" data
    std::string target_str = "{\"geometry\": null, \"tags\": {\"id\": 1}}";
    std::vector<uint8_t> target_bytes(target_str.begin(), target_str.end());
    Selector actual("actual/data", {{"id", 1}});
    vfs.write(actual, target_bytes);

    // Create a link: alias/data -> actual/data
    Selector alias("alias/data", {{"id", 1}});
    vfs.link(alias, actual);

    // Read via the link
    auto resolved = vfs.read<json>(alias);
    assert(resolved["tags"]["id"] == 1);
    std::cout << "✅ Link Resolution PASS" << std::endl;
    std::filesystem::remove_all(config.storage_dir);
}

void test_vectorized_expansion() {
    std::cout << "Running test_vectorized_expansion..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_logic_test_vectorized";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    Processor::register_op<BoxOp<>, Interval, Interval, Interval>(&vfs, "jot/Box");

    // BoxOp now takes Intervals. Testing basic call via Processor.
    Selector sel("jot/Box", {{"width", 10.0}, {"height", 20.0}, {"depth", 0.1}});
    Shape s = vfs.read<Shape>(sel.with_output("$out"));
    
    assert(s.tags["type"] == "box");
    std::cout << "✅ Basic Processor Execution PASS" << std::endl;
    std::filesystem::remove_all(config.storage_dir);
}

void test_nth_resolution() {
    std::cout << "Running test_nth_resolution..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_logic_test_nth";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    Processor::register_op<NthOp<>, Shape, std::vector<double>>(&vfs, "jot/nth");

    // Create a mock group shape
    Shape s0; s0.add_tag("id", "zero");
    Shape s1; s1.add_tag("id", "one");
    
    Shape group_shape;
    group_shape.components = {s0, s1};

    Selector mock_group_sel("op/mock_group");
    vfs.write(mock_group_sel, group_shape);

    // Selector for nth
    Selector nth_sel("jot/nth", {{"$in", mock_group_sel.to_json()}, {"index", {1.0}}});

    Shape result = vfs.read<Shape>(nth_sel.with_output("$out"));

    assert(result.tags["id"] == "one");
    std::cout << "✅ Nth Resolved and Selected Index 1 PASS" << std::endl;

    std::filesystem::remove_all(config.storage_dir);
}

void test_hexagon_scale() {
    std::cout << "Running test_hexagon_scale..." << std::endl;
    VFSNode::Config config;
    config.id = "test-node";
    config.storage_dir = ".vfs_storage_logic_test_hexagon";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    Processor::register_op<HexagonOp<>::ByEdgeToEdge, double, double>(&vfs, "jot/Hexagon/edgeToEdge");

    double e2e = 248.0;
    Selector sel("jot/Hexagon/edgeToEdge", {{"edgeToEdge", e2e}, {"turns", 0.0}});
    Shape s = vfs.read<Shape>(sel.with_output("$out"));
    
    // Read the geometry to check vertex coordinates
    assert(s.geometry.has_value());
    Geometry g = vfs.read<Geometry>(*s.geometry);
    
    std::cout << "Hexagon vertices count: " << g.vertices.size() << std::endl;
    double r = 0;
    for (size_t i = 0; i < g.vertices.size(); ++i) {
        double x = CGAL::to_double(g.vertices[i].x);
        double y = CGAL::to_double(g.vertices[i].y);
        std::cout << "Vertex " << i << ": (" << x << ", " << y << ")" << std::endl;
        if (i == 0) r = x; 
    }

    // Radius should be e2e / sqrt(3) approx 143.18
    std::cout << "Calculated Radius: " << r << " (Expected ~143.18)" << std::endl;
    assert(std::abs(r - 143.18) < 0.1);
    
    std::cout << "✅ Hexagon Scale PASS" << std::endl;
    std::filesystem::remove_all(config.storage_dir);
}

int main() {
    try {
        test_throwing_read();
        test_formal_links();
        test_vectorized_expansion();
        test_nth_resolution();
        test_hexagon_scale();
        std::cout << "\nALL LOGIC TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
