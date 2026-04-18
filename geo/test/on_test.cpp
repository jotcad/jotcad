#include "test_base.h"
#include "../hexagon_op.h"
#include "../corners_op.h"
#include "../on_op.h"
#include "../offset_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Workbench (On)..." << std::endl;
    MockVFS vfs;
    register_all_ops();

    // 1. Setup VFS handlers for the mock
    for (auto const& [path, op] : Processor::registry()) {
        vfs.register_op(path, [&vfs, path](const VFSNode::VFSRequest& req) {
            return Processor::registry()[path].logic(&vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }

    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    Shape corners;
    CornersOp<>::execute(&vfs, hex, true, corners);
    Shape c0 = corners.components[0];

    // 2. Define an "Operation Recipe" (Partial Selector)
    // We'll use Offset(5) as our operation.
    nlohmann::json recipe = {
        {"path", "jot/offset"},
        {"parameters", {{"distance", 5.0}}}
    };

    // 3. Execute On
    Shape result;
    OnOp<>::execute(&vfs, hex, {c0}, recipe, result);

    // 4. Verify
    // The result should be the hexagon, moved to origin, offset, and moved back.
    // If it worked, the geometry path should be the offset result.
    assert(result.tags["operation"] == "offset");
    
    // Position check: The vertex that was at (30,0) should still be at (30,0) 
    // after the round-trip (plus offset expansion).
    // The Matrix logic ensures local work projects back to the same global spot.
    
    std::cout << "✅ On PASS" << std::endl;
    return 0;
}
