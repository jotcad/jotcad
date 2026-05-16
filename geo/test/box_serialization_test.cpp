#include "test_base.h"
#include "boolean/engine.h"
#include <iomanip>

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

int main() {
    std::cout << "Verifying Serialization Impact on Box Manifoldness..." << std::endl;

    // 1. Create a perfect 10x10x10 Box
    Geometry box;
    double x_min = -5, x_max = 5;
    double y_min = -5, y_max = 5;
    double z_min = -5, z_max = 5;

    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)});
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_min)});
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)});
    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)});
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_max)});
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_max)});
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_max)});

    box.faces.push_back({{{3, 2, 1, 0}}});
    box.faces.push_back({{{4, 5, 6, 7}}});
    box.faces.push_back({{{0, 1, 5, 4}}});
    box.faces.push_back({{{1, 2, 6, 5}}});
    box.faces.push_back({{{2, 3, 7, 6}}});
    box.faces.push_back({{{3, 0, 4, 7}}});

    // 2. Simulate VFS Serialization (Default precision)
    std::string data = box.encode_text();
    Geometry decoded;
    decoded.decode_text(data);

    // 3. Test Manifoldness of decoded geometry
    std::cout << "Testing Decoded Geometry..." << std::endl;
    ExactMesh mesh = Engine::geometry_to_mesh(decoded);
    
    bool closed = CGAL::is_closed(mesh);
    std::cout << "  - Mesh Closed: " << (closed ? "YES" : "NO") << std::endl;

    if (!closed) {
        std::cout << "  - FAILURE detected. Investigating vertex drift..." << std::endl;
        for (size_t i = 0; i < decoded.vertices.size(); ++i) {
            std::cout << "    V[" << i << "]: " << std::setprecision(20) << decoded.vertices[i].x << std::endl;
        }
    } else {
        std::cout << "  - SUCCESS. Default serialization preserved the manifold." << std::endl;
    }

    return 0;
}
