#include "test_base.h"
#include "../box_op.h"
#include "../corners_op.h"
#include "../on_op.h"
#include "../cut_op.h"
#include <chrono>

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Sequential Accumulator (On)..." << std::endl;
    
    // Setup VFS
    std::string test_storage = ".vfs_storage_acc_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(test_storage);
    VFSNode::Config mock_config = {"mock_acc", "0.0.1", test_storage, {}, 0};
    MockVFS vfs;
    vfs.config() = mock_config;

    register_all_ops();
    for (auto const& [path, op] : Processor::registry()) {
        vfs.register_op(path, [&vfs, op](const VFSNode::VFSRequest& req) {
            return op.logic(&vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }

    // 1. Create a large box (100x100) - Use a single rectangle face
    Shape base;
    Geometry base_geo; makeRectangle(base_geo, 100, 100);
    // Center it manually
    FT half(50);
    for (auto& v : base_geo.vertices) { v.x -= half; v.y -= half; }
    base = JotVfsProtocol::make_shape(&vfs, base_geo, {{"type","box"},{"plane","Z0"}});
    
    // 2. Extract 1st and 2nd corners (adjacent)
    Shape corners;
    CornersOp<>::execute(&vfs, base, false, corners);
    std::vector<Shape> targets = {corners.components[0], corners.components[1]};

    // 3. Define a "Drill" operation (10x10 hole, moved so it's fully inside the square)
    Shape tool;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, tool);
    // Move tool to (30,30) relative to workbench origin
    tool.tf = Matrix::translate(30, 30, 0).to_vec();
    
    nlohmann::json recipe = {
        {"path", "jot/cut"},
        {"parameters", {{"tools", {tool.to_json()}}}}
    };

    // 4. Execute Sequential On
    Shape result;
    OnOp<>::execute(&vfs, base, targets, recipe, result);

    // 5. Verify
    Geometry geo = vfs.read_geo(result.geometry);
    
    // We expect ONE face with 3 loops (Outer + 2 Holes)
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 3);
    // 4 (base) + 4 (hole1) + 4 (hole2) = 12 vertices
    assert(geo.vertices.size() == 12);
    
    std::cout << "✅ Accumulator PASS" << std::endl;
    std::filesystem::remove_all(test_storage);
    return 0;
}
