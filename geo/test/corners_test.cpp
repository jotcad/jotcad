#include "test_base.h"
#include "../hexagon_op.h"
#include "../corners_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Corners Extraction..." << std::endl;
    MockVFS vfs;
    
    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    Shape corners;
    CornersOp<>::execute(&vfs, hex, true, corners);
    
    assert(corners.components.size() == 6);
    
    // Check first corner orientation
    Shape c0 = corners.components[0];
    assert(c0.tags["type"] == "corner");
    assert(c0.tags["index"] == 0);
    
    // c0.tf should map origin to a point at distance 15.0 from center
    double tx = c0.tf[3];
    double ty = c0.tf[7];
    double tz = c0.tf[11];
    double dist = std::sqrt(tx*tx + ty*ty + tz*tz);
    assert(std::abs(dist - 15.0) < 1e-6);
    
    // Geometry check
    Geometry geo = vfs.read_geo(c0.geometry);
    assert(geo.vertices.size() == 3); // V-Proxy
    
    std::cout << "✅ Corners PASS" << std::endl;
    return 0;
}
