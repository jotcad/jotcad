#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("sew");
    register_all_ops(&vfs);
    
    std::cout << "Testing Sew Operator..." << std::endl;
    
    // 1. Sew independent faces into a cube
    // Build a 10x10 cube from 6 faces
    std::vector<Shape> cube_faces;
    double s = 5.0;
    
    auto make_face = [&](Matrix tf) {
        Geometry f;
        f.vertices = {{FT(-s), FT(-s), FT(0)}, {FT(s), FT(-s), FT(0)}, {FT(s), FT(s), FT(0)}, {FT(-s), FT(s), FT(0)}};
        f.faces.push_back({{{0, 1, 2, 3}}});
        Shape sh = JotVfsProtocol::make_shape(&vfs, f, {{"type", "face"}});
        sh.tf = tf;
        return sh;
    };

    cube_faces.push_back(make_face(Matrix::translate(0, 0, s)));  // Top
    cube_faces.push_back(make_face(Matrix::translate(0, 0, -s))); // Bottom
    cube_faces.push_back(make_face(Matrix::rotationX(0.25) * Matrix::translate(0, 0, s)));  // Front
    cube_faces.push_back(make_face(Matrix::rotationX(-0.25) * Matrix::translate(0, 0, s))); // Back
    cube_faces.push_back(make_face(Matrix::rotationY(0.25) * Matrix::translate(0, 0, s)));  // Right
    cube_faces.push_back(make_face(Matrix::rotationY(-0.25) * Matrix::translate(0, 0, s))); // Left

    fs::Selector sew_addr = fs::Selector{"jot/Sew", {{"shapes", cube_faces}}}.with_output("$out");
    Processor::execute(&vfs, sew_addr);
    Shape s_out = vfs.read<Shape>(sew_addr);
    Geometry geo_out = vfs.read<Geometry>(s_out.geometry.value());

    std::cout << "  - Cube Sew: " << geo_out.faces.size() << " faces." << std::endl;
    assert(geo_out.faces.size() == 6);
    
    // Verify well-formedness (Closure and Outward Orientation)
    vfs.verify_well_formed_solid(geo_out, "sewn_cube");

    // 2. Sew an open sheet (2 faces)
    std::vector<Shape> sheet_faces;
    sheet_faces.push_back(make_face(Matrix::translate(-s, 0, 0)));
    sheet_faces.push_back(make_face(Matrix::translate(s, 0, 0)));
    
    fs::Selector sheet_addr = fs::Selector{"jot/Sew", {{"shapes", sheet_faces}}}.with_output("$out");
    Processor::execute(&vfs, sheet_addr);
    Shape s_sheet = vfs.read<Shape>(sheet_addr);
    Geometry geo_sheet = vfs.read<Geometry>(s_sheet.geometry.value());
    
    std::cout << "  - Sheet Sew: " << geo_sheet.faces.size() << " faces." << std::endl;
    assert(geo_sheet.faces.size() == 2);
    // Check orientation bias (Normals should point +Z by default)
    auto v0 = geo_sheet.vertices[geo_sheet.faces[0].loops[0][0]];
    auto v1 = geo_sheet.vertices[geo_sheet.faces[0].loops[0][1]];
    auto v2 = geo_sheet.vertices[geo_sheet.faces[0].loops[0][2]];
    Vector_3 n = CGAL::cross_product(Point_3(v1.x, v1.y, v1.z) - Point_3(v0.x, v0.y, v0.z),
                                    Point_3(v2.x, v2.y, v2.z) - Point_3(v1.x, v1.y, v1.z));
    std::cout << "  - Sheet Normal: " << n << std::endl;
    assert(n.z() > 0);

    std::cout << "✅ ALL Sew Tests Passed" << std::endl;
    return 0;
}
