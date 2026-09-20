#include "test_base.h"
#include "boolean/engine.h"
#include <cassert>
#include <iomanip>

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

int main() {
    std::cout << "Testing Exact Rational Serialization for Geometry and Matrix..." << std::endl;

    // 1. Test Exact Rational Geometry Serialization
    std::cout << "  - Testing exact rational Geometry serialization..." << std::endl;
    Geometry g;
    // Non-trivial rational coordinates that cannot be represented in binary floating point without precision loss
    g.vertices.push_back({FT(1) / 3, FT(7) / 11, FT(-5) / 13});
    g.vertices.push_back({FT(10) / 3, FT(7) / 11, FT(-5) / 13});
    g.vertices.push_back({FT(1) / 3, FT(40) / 11, FT(-5) / 13});
    g.triangles.push_back({0, 1, 2});

    std::string encoded_g = g.encode_text();
    // Verify that the serialization actually contains rational fraction slashes
    assert(encoded_g.find("1/3") != std::string::npos);
    assert(encoded_g.find("7/11") != std::string::npos);
    assert(encoded_g.find("-5/13") != std::string::npos);

    Geometry decoded_g;
    decoded_g.decode_text(encoded_g);
    assert(decoded_g.vertices.size() == 3);
    assert(decoded_g.triangles.size() == 1);
    // EXACT rational equality check: must be bitwise identical in EK::FT
    assert(decoded_g.vertices[0].x == FT(1) / 3);
    assert(decoded_g.vertices[0].y == FT(7) / 11);
    assert(decoded_g.vertices[0].z == FT(-5) / 13);
    assert(decoded_g.vertices[1].x == FT(10) / 3);
    assert(decoded_g.vertices[2].y == FT(40) / 11);
    std::cout << "    - Confirmed Geometry rational fractions preserved with 0% drift." << std::endl;

    // 2. Test Exact Rational Matrix Serialization
    std::cout << "  - Testing exact rational Matrix serialization..." << std::endl;
    // Rotation with non-trivial rational coefficients
    Matrix rot = Matrix::rotationZ(1.0 / 6.0); // 30 deg rational rotation approximation
    Matrix trans = Matrix::translate(FT(2) / 7, FT(5) / 9, FT(-3) / 17);
    Matrix tf = trans * rot;

    std::string tf_str = tf.to_vec();
    // Must contain rational fraction slashes
    assert(tf_str.find('/') != std::string::npos);
    assert(tf_str.find("2/7") != std::string::npos);
    assert(tf_str.find("5/9") != std::string::npos);
    assert(tf_str.find("-3/17") != std::string::npos);

    Matrix tf_decoded = Matrix::from_vec(tf_str);
    assert(tf_decoded.to_vec() == tf_str);
    std::cout << "    - Confirmed Matrix rational coefficients preserved with 0% drift." << std::endl;

    // 3. Test Full Mesh Serialization & Manifold Preservation on Rotated Model
    std::cout << "  - Testing full 3D mesh serialization & manifold preservation under tilt..." << std::endl;
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

    ExactMesh m = Engine::geometry_to_mesh(box);
    // Apply compound 3D tilt
    Matrix tilt = Matrix::rotationX(1.0 / 7.0) * Matrix::rotationY(1.0 / 5.0) * Matrix::rotationZ(1.0 / 3.0);
    Engine::transform_mesh(m, tilt);

    Geometry tilted_geo = Engine::mesh_to_geometry(m);
    std::string tilted_data = tilted_geo.encode_text();
    assert(tilted_data.find('/') != std::string::npos);

    Geometry tilted_decoded;
    tilted_decoded.decode_text(tilted_data);
    ExactMesh reconstructed = Engine::geometry_to_mesh(tilted_decoded);

    fix::assert_well_formed_closed_mesh(reconstructed, "reconstructed tilted box mesh in box_serialization_test");
    std::cout << "    - Confirmed rotated mesh round-trips through exact serialization without self-intersections." << std::endl;

    std::cout << "✅ ALL Rational Serialization Tests Passed." << std::endl;
    return 0;
}
