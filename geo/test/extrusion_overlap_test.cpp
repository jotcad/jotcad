#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/intersection.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;

typedef ::CGAL::Surface_mesh<::jotcad::geo::IK::Point_3> Mesh;
typedef boolean::ExactMesh ExactMesh;

// Matrix for rotating the local coordinate system
struct Matrix3x3 {
    double m[3][3];
    static Matrix3x3 rotation_z_to_v(double vx, double vy, double vz) {
        Matrix3x3 R;
        double len = std::sqrt(vx*vx + vy*vy + vz*vz);
        if (len > 1e-12) { vx /= len; vy /= len; vz /= len; }
        
        if (vz > 0.99999) {
            R.m[0][0] = 1; R.m[0][1] = 0; R.m[0][2] = 0;
            R.m[1][0] = 0; R.m[1][1] = 1; R.m[1][2] = 0;
            R.m[2][0] = 0; R.m[2][1] = 0; R.m[2][2] = 1;
        } else if (vz < -0.99999) {
            R.m[0][0] = 1; R.m[0][1] = 0; R.m[0][2] = 0;
            R.m[1][0] = 0; R.m[1][1] = -1; R.m[1][2] = 0;
            R.m[2][0] = 0; R.m[2][1] = 0; R.m[2][2] = -1;
        } else {
            double ax = -vy;
            double ay = vx;
            double alen = std::sqrt(ax*ax + ay*ay);
            ax /= alen; ay /= alen;
            
            double cos_t = vz;
            double sin_t = std::sqrt(1.0 - cos_t*cos_t);
            double one_minus_cos = 1.0 - cos_t;
            
            R.m[0][0] = cos_t + ax*ax*one_minus_cos;
            R.m[0][1] = ax*ay*one_minus_cos;
            R.m[0][2] = ay*sin_t;
            
            R.m[1][0] = ax*ay*one_minus_cos;
            R.m[1][1] = cos_t + ay*ay*one_minus_cos;
            R.m[1][2] = -ax*sin_t;
            
            R.m[2][0] = -ay*sin_t;
            R.m[2][1] = ax*sin_t;
            R.m[2][2] = cos_t;
        }
        return R;
    }
    
    IK::Point_3 local_to_world(double lx, double ly, double lz) const {
        double wx = m[0][0]*lx + m[0][1]*ly + m[0][2]*lz;
        double wy = m[1][0]*lx + m[1][1]*ly + m[1][2]*lz;
        double wz = m[2][0]*lx + m[2][1]*ly + m[2][2]*lz;
        return IK::Point_3(wx, wy, wz);
    }
    
    IK::Point_3 world_to_local(double wx, double wy, double wz) const {
        double lx = m[0][0]*wx + m[1][0]*wy + m[2][0]*wz;
        double ly = m[0][1]*wx + m[1][1]*wy + m[2][1]*wz;
        double lz = m[0][2]*wx + m[1][2]*wy + m[2][2]*wz;
        return IK::Point_3(lx, ly, lz);
    }
};

// Build an axis-aligned box geometry
Geometry build_box_geo(double xmin, double xmax, double ymin, double ymax, double zmin, double zmax) {
    Geometry geo;
    geo.vertices.push_back({xmin, ymin, zmin});
    geo.vertices.push_back({xmax, ymin, zmin});
    geo.vertices.push_back({xmax, ymax, zmin});
    geo.vertices.push_back({xmin, ymax, zmin});
    geo.vertices.push_back({xmin, ymin, zmax});
    geo.vertices.push_back({xmax, ymin, zmax});
    geo.vertices.push_back({xmax, ymax, zmax});
    geo.vertices.push_back({xmin, ymax, zmax});

    geo.triangles.push_back({0, 3, 2});
    geo.triangles.push_back({0, 2, 1});
    geo.triangles.push_back({4, 5, 6});
    geo.triangles.push_back({4, 6, 7});
    geo.triangles.push_back({0, 1, 5});
    geo.triangles.push_back({0, 5, 4});
    geo.triangles.push_back({2, 3, 7});
    geo.triangles.push_back({2, 7, 6});
    geo.triangles.push_back({3, 0, 4});
    geo.triangles.push_back({3, 4, 7});
    geo.triangles.push_back({1, 2, 6});
    geo.triangles.push_back({1, 6, 5});
    return geo;
}

void run_extrusion_overlap_test() {
    MockVFS vfs("extrusion_overlap");
    register_all_ops(&vfs);

    std::cout << "Testing Demoldability via Planar Split Mold Blocks with Inserts..." << std::endl;

    // 1. Load the bear STL
    std::string bear_path = "scratch/bear.stl";
    {
        std::ifstream check(bear_path);
        if (!check.good()) {
            check.open("../scratch/bear.stl");
            if (check.good()) {
                bear_path = "../scratch/bear.stl";
            } else {
                bear_path = "../../scratch/bear.stl";
            }
        }
    }
    Geometry bear_geo;
    if (!STLReader::read_file(bear_path, bear_geo)) {
        std::cerr << "  ❌ FAIL: Could not load bear.stl" << std::endl;
        return;
    }

    // Find bear bounding box to size the mold block
    double b_xmin = 1e9, b_xmax = -1e9;
    double b_ymin = 1e9, b_ymax = -1e9;
    double b_zmin = 1e9, b_zmax = -1e9;
    for (const auto& v : bear_geo.vertices) {
        double vx = CGAL::to_double(v.x);
        double vy = CGAL::to_double(v.y);
        double vz = CGAL::to_double(v.z);
        b_xmin = std::min(b_xmin, vx); b_xmax = std::max(b_xmax, vx);
        b_ymin = std::min(b_ymin, vy); b_ymax = std::max(b_ymax, vy);
        b_zmin = std::min(b_zmin, vz); b_zmax = std::max(b_zmax, vz);
    }

    // 2. Define main draw vector and parting plane (X = 0)
    double dx = 1.0, dy = 0.0, dz = 0.0;
    ::jotcad::geo::IK::Vector_3 dir_ik(dx, dy, dz);

    // Compute face normals and centroids
    std::vector<::jotcad::geo::IK::Vector_3> face_normals(bear_geo.triangles.size());
    std::vector<::jotcad::geo::IK::Point_3> face_centroids(bear_geo.triangles.size());
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        const auto& p0 = bear_geo.vertices[tri[0]];
        const auto& p1 = bear_geo.vertices[tri[1]];
        const auto& p2 = bear_geo.vertices[tri[2]];
        
        double ux = ::CGAL::to_double(p1.x - p0.x);
        double uy = ::CGAL::to_double(p1.y - p0.y);
        double uz = ::CGAL::to_double(p1.z - p0.z);

        double vx = ::CGAL::to_double(p2.x - p0.x);
        double vy = ::CGAL::to_double(p2.y - p0.y);
        double vz = ::CGAL::to_double(p2.z - p0.z);

        double nx = uy * vz - uz * vy;
        double ny = uz * vx - ux * vz;
        double nz = ux * vy - uy * vx;

        double flen = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (flen > 1e-12) {
            nx /= flen; ny /= flen; nz /= flen;
        }
        face_normals[f_idx] = ::jotcad::geo::IK::Vector_3(nx, ny, nz);

        double cx = ::CGAL::to_double(p0.x + p1.x + p2.x) / 3.0;
        double cy = ::CGAL::to_double(p0.y + p1.y + p2.y) / 3.0;
        double cz = ::CGAL::to_double(p0.z + p1.z + p2.z) / 3.0;
        face_centroids[f_idx] = ::jotcad::geo::IK::Point_3(cx, cy, cz);
    }

    // 3. Build AABB Tree of the bear to identify trapped faces along main draw direction
    Mesh bear_mesh = boolean::Engine::geometry_to_mesh_ik(bear_geo);
    typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
    typedef CGAL::AABB_traits<IK, Primitive> AABB_Traits;
    typedef CGAL::AABB_tree<AABB_Traits> Tree;

    Tree tree(CGAL::faces(bear_mesh).first, CGAL::faces(bear_mesh).second, bear_mesh);
    tree.build();

    // 4. Identify trapped (undercut/shadowed) faces relative to X draw vector
    std::vector<bool> is_trapped(bear_geo.triangles.size(), false);
    int trapped_count = 0;

    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        double dot = ::CGAL::to_double(face_normals[f_idx] * dir_ik);
        ::jotcad::geo::IK::Vector_3 ray_dir = (dot >= 0.0) ? dir_ik : -dir_ik;
        ::jotcad::geo::IK::Ray_3 ray(face_centroids[f_idx], ray_dir);

        std::vector<typename Tree::Intersection_and_primitive_id<::jotcad::geo::IK::Ray_3>::Type> intersections;
        tree.all_intersections(ray, std::back_inserter(intersections));

        for (const auto& inter : intersections) {
            ::jotcad::geo::IK::Point_3 pt;
            if (const ::jotcad::geo::IK::Point_3* pi = std::get_if<::jotcad::geo::IK::Point_3>(&inter.first)) {
                pt = *pi;
            } else if (const ::jotcad::geo::IK::Segment_3* ps = std::get_if<::jotcad::geo::IK::Segment_3>(&inter.first)) {
                pt = ps->source();
            } else {
                continue;
            }

            if (::CGAL::to_double(::CGAL::squared_distance(pt, face_centroids[f_idx])) > 1e-4) {
                is_trapped[f_idx] = true;
                trapped_count++;
                break;
            }
        }
    }

    std::cout << "  - Detected " << trapped_count << " trapped triangles out of " 
              << bear_geo.triangles.size() << " total along [1, 0, 0]." << std::endl;

    // Collect trapped faces and compute their local release vector
    std::vector<int> insert_faces;
    ::jotcad::geo::IK::Vector_3 avg_insert_normal(0, 0, 0);
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        if (is_trapped[f_idx]) {
            insert_faces.push_back((int)f_idx);
            avg_insert_normal = avg_insert_normal + face_normals[f_idx];
        }
    }

    double ilen = std::sqrt(::CGAL::to_double(avg_insert_normal.squared_length()));
    double idx = 0.0, idy = 0.0, idz = 1.0;
    if (ilen > 1e-12) {
        idx = ::CGAL::to_double(avg_insert_normal.x()) / ilen;
        idy = ::CGAL::to_double(avg_insert_normal.y()) / ilen;
        idz = ::CGAL::to_double(avg_insert_normal.z()) / ilen;
    }
    ::jotcad::geo::IK::Vector_3 insert_dir_ik(idx, idy, idz);
    std::cout << "  - Local Insert Draw Direction: [" << idx << ", " << idy << ", " << idz << "]" << std::endl;

    // 5. Define the Left and Right mold block boxes split planarly along X = 0
    double pad = 10.0;
    double mx_min = b_xmin - pad, mx_max = b_xmax + pad;
    double my_min = b_ymin - pad, my_max = b_ymax + pad;
    double mz_min = b_zmin - pad, mz_max = b_zmax + pad;

    Geometry b_left_geo = build_box_geo(mx_min, 0.0, my_min, my_max, mz_min, mz_max);
    Geometry b_right_geo = build_box_geo(0.0, mx_max, my_min, my_max, mz_min, mz_max);

    // 6. Build the slide block box aligned with insert_dir
    std::set<int> insert_vertices;
    for (int f_idx : insert_faces) {
        insert_vertices.insert(bear_geo.triangles[f_idx][0]);
        insert_vertices.insert(bear_geo.triangles[f_idx][1]);
        insert_vertices.insert(bear_geo.triangles[f_idx][2]);
    }

    double l_xmin = 1e9, l_xmax = -1e9;
    double l_ymin = 1e9, l_ymax = -1e9;
    double l_zmin = 1e9, l_zmax = -1e9;
    Matrix3x3 R = Matrix3x3::rotation_z_to_v(idx, idy, idz);
    for (int v_idx : insert_vertices) {
        const auto& v = bear_geo.vertices[v_idx];
        IK::Point_3 l = R.world_to_local(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
        l_xmin = std::min(l_xmin, l.x());
        l_xmax = std::max(l_xmax, l.x());
        l_ymin = std::min(l_ymin, l.y());
        l_ymax = std::max(l_ymax, l.y());
        l_zmin = std::min(l_zmin, l.z());
        l_zmax = std::max(l_zmax, l.z());
    }

    double bpad = 1.0;
    l_xmin -= bpad; l_xmax += bpad; l_ymin -= bpad; l_ymax += bpad;
    l_zmin -= bpad;
    double l_zmax_ext = l_zmax + 100.0; // Extend outwards past the mold block boundary

    std::vector<IK::Point_3> box_vertices(8);
    box_vertices[0] = R.local_to_world(l_xmin, l_ymin, l_zmin);
    box_vertices[1] = R.local_to_world(l_xmax, l_ymin, l_zmin);
    box_vertices[2] = R.local_to_world(l_xmax, l_ymax, l_zmin);
    box_vertices[3] = R.local_to_world(l_xmin, l_ymax, l_zmin);
    box_vertices[4] = R.local_to_world(l_xmin, l_ymin, l_zmax_ext);
    box_vertices[5] = R.local_to_world(l_xmax, l_ymin, l_zmax_ext);
    box_vertices[6] = R.local_to_world(l_xmax, l_ymax, l_zmax_ext);
    box_vertices[7] = R.local_to_world(l_xmin, l_ymax, l_zmax_ext);

    Geometry slide_box_geo;
    for (int i = 0; i < 8; ++i) {
        slide_box_geo.vertices.push_back({box_vertices[i].x(), box_vertices[i].y(), box_vertices[i].z()});
    }
    slide_box_geo.triangles.push_back({0, 3, 2});
    slide_box_geo.triangles.push_back({0, 2, 1});
    slide_box_geo.triangles.push_back({4, 5, 6});
    slide_box_geo.triangles.push_back({4, 6, 7});
    slide_box_geo.triangles.push_back({0, 1, 5});
    slide_box_geo.triangles.push_back({0, 5, 4});
    slide_box_geo.triangles.push_back({2, 3, 7});
    slide_box_geo.triangles.push_back({2, 7, 6});
    slide_box_geo.triangles.push_back({3, 0, 4});
    slide_box_geo.triangles.push_back({3, 4, 7});
    slide_box_geo.triangles.push_back({1, 2, 6});
    slide_box_geo.triangles.push_back({1, 6, 5});

    // 7. Perform Boolean cuts on simple clean solid inputs
    ExactMesh mesh_bear = boolean::Engine::geometry_to_mesh(bear_geo);
    ExactMesh mesh_slide_box = boolean::Engine::geometry_to_mesh(slide_box_geo);
    ExactMesh mesh_b_left = boolean::Engine::geometry_to_mesh(b_left_geo);
    ExactMesh mesh_b_right = boolean::Engine::geometry_to_mesh(b_right_geo);

    // Compute Insert: SlideBox - Bear
    std::cout << "  - Computing Insert: SlideBox - Bear..." << std::endl;
    ExactMesh mesh_insert = mesh_slide_box;
    boolean::Engine::cut_mesh_by_mesh(mesh_insert, mesh_bear);

    // Compute Raw Left Half: LeftBox - Bear
    std::cout << "  - Computing Raw Left: LeftBox - Bear..." << std::endl;
    ExactMesh mesh_left_raw = mesh_b_left;
    boolean::Engine::cut_mesh_by_mesh(mesh_left_raw, mesh_bear);

    // Compute Raw Right Half: RightBox - Bear
    std::cout << "  - Computing Raw Right: RightBox - Bear..." << std::endl;
    ExactMesh mesh_right_raw = mesh_b_right;
    boolean::Engine::cut_mesh_by_mesh(mesh_right_raw, mesh_bear);

    // Compute Final Left Half: LeftRaw - SlideBox
    std::cout << "  - Computing Final Left: LeftRaw - SlideBox..." << std::endl;
    ExactMesh mesh_left = mesh_left_raw;
    boolean::Engine::cut_mesh_by_mesh(mesh_left, mesh_slide_box);

    // Compute Final Right Half: RightRaw - SlideBox
    std::cout << "  - Computing Final Right: RightRaw - SlideBox..." << std::endl;
    ExactMesh mesh_right = mesh_right_raw;
    boolean::Engine::cut_mesh_by_mesh(mesh_right, mesh_slide_box);

    // Convert back to Geometry
    Geometry left_final_geo = boolean::Engine::mesh_to_geometry(mesh_left);
    Geometry right_final_geo = boolean::Engine::mesh_to_geometry(mesh_right);
    Geometry insert_final_geo = boolean::Engine::mesh_to_geometry(mesh_insert);

    // Write STL files for visual check
    std::filesystem::create_directories("actual");
    
    auto write_stl = [&](const Geometry& geo, const std::string& filepath) {
        STLWriter writer;
        writer.add_geometry(geo);
        auto bin = writer.write_binary();
        std::ofstream out(filepath, std::ios::binary);
        out.write((const char*)bin.data(), bin.size());
    };

    write_stl(left_final_geo, "actual/bear_left_extruded.stl");
    write_stl(right_final_geo, "actual/bear_right_extruded.stl");
    write_stl(insert_final_geo, "actual/bear_insert_extruded.stl");
    std::cout << "  📸 Saved 3-piece STLs to actual/bear_*_extruded.stl" << std::endl;

    // Render visual confirmation showing all 3 pieces
    Shape left_shape = JotVfsProtocol::make_shape(&vfs, left_final_geo, {{"color", "#2bee2b"}, {"name", "left_extrusion"}});
    Shape right_shape = JotVfsProtocol::make_shape(&vfs, right_final_geo, {{"color", "#ee2b2b"}, {"name", "right_extrusion"}});
    Shape insert_shape = JotVfsProtocol::make_shape(&vfs, insert_final_geo, {{"color", "#2b2bee"}, {"name", "insert_extrusion"}});

    Shape composite;
    composite.components.push_back(left_shape);
    composite.components.push_back(right_shape);
    composite.components.push_back(insert_shape);

    composite.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

    auto png_data = Rasterizer::render_png(&vfs, composite, 1024, 1024, 0.0, 0.0);
    if (!png_data.empty()) {
        std::ofstream out("actual/bear_extrusions.png", std::ios::binary);
        out.write((const char*)png_data.data(), png_data.size());
        std::cout << "  📸 Saved actual/bear_extrusions.png" << std::endl;
    }

    // Convert back to InexactMesh for verification checks
    Mesh mesh_left_ik = boolean::Engine::geometry_to_mesh_ik(left_final_geo);
    Mesh mesh_right_ik = boolean::Engine::geometry_to_mesh_ik(right_final_geo);
    Mesh mesh_insert_ik = boolean::Engine::geometry_to_mesh_ik(insert_final_geo);

    // Verify topological validity and no self-intersections
    bool left_valid = ::CGAL::is_valid_polygon_mesh(mesh_left_ik);
    bool right_valid = ::CGAL::is_valid_polygon_mesh(mesh_right_ik);
    bool insert_valid = ::CGAL::is_valid_polygon_mesh(mesh_insert_ik);
    std::cout << "  - Left swept volume topological validity: " << (left_valid ? "✅ VALID" : "❌ INVALID") << std::endl;
    std::cout << "  - Right swept volume topological validity: " << (right_valid ? "✅ VALID" : "❌ INVALID") << std::endl;
    std::cout << "  - Insert swept volume topological validity: " << (insert_valid ? "✅ VALID" : "❌ INVALID") << std::endl;

    bool left_self_intersects = ::CGAL::Polygon_mesh_processing::does_self_intersect(mesh_left_ik);
    bool right_self_intersects = ::CGAL::Polygon_mesh_processing::does_self_intersect(mesh_right_ik);
    bool insert_self_intersects = ::CGAL::Polygon_mesh_processing::does_self_intersect(mesh_insert_ik);

    std::cout << "  - Left swept volume self-intersection check: " << (left_self_intersects ? "❌ FAILED" : "✅ PASSED") << std::endl;
    std::cout << "  - Right swept volume self-intersection check: " << (right_self_intersects ? "❌ FAILED" : "✅ PASSED") << std::endl;
    std::cout << "  - Insert swept volume self-intersection check: " << (insert_self_intersects ? "❌ FAILED" : "✅ PASSED") << std::endl;

    if (left_self_intersects || right_self_intersects || insert_self_intersects) {
        std::cerr << "  ❌ FAIL: Swept volume extrusions contain self-intersections." << std::endl;
        return;
    }

    // Verify no overlap between the 3 swept volumes when shifted along their pull vectors
    Mesh mesh_left_shifted = mesh_left_ik;
    Mesh mesh_right_shifted = mesh_right_ik;
    Mesh mesh_insert_shifted = mesh_insert_ik;

    // Pull main halves apart along X axis
    ::jotcad::geo::IK::Vector_3 shift_left(-0.2, 0.0, 0.0);
    ::jotcad::geo::IK::Vector_3 shift_right(0.2, 0.0, 0.0);
    // Pull insert along its local draw axis
    ::jotcad::geo::IK::Vector_3 shift_insert(0.2 * idx, 0.2 * idy, 0.2 * idz);

    for (auto v : mesh_left_shifted.vertices()) {
        mesh_left_shifted.point(v) = mesh_left_shifted.point(v) + shift_left;
    }
    for (auto v : mesh_right_shifted.vertices()) {
        mesh_right_shifted.point(v) = mesh_right_shifted.point(v) + shift_right;
    }
    for (auto v : mesh_insert_shifted.vertices()) {
        mesh_insert_shifted.point(v) = mesh_insert_shifted.point(v) + shift_insert;
    }

    bool overlap_lr = ::CGAL::Polygon_mesh_processing::do_intersect(mesh_left_shifted, mesh_right_shifted);
    bool overlap_li = ::CGAL::Polygon_mesh_processing::do_intersect(mesh_left_shifted, mesh_insert_shifted);
    bool overlap_ri = ::CGAL::Polygon_mesh_processing::do_intersect(mesh_right_shifted, mesh_insert_shifted);

    std::cout << "  - Shifted Left vs Right intersection: " << (overlap_lr ? "❌ OVERLAPPED" : "✅ NO OVERLAP") << std::endl;
    std::cout << "  - Shifted Left vs Insert intersection: " << (overlap_li ? "❌ OVERLAPPED" : "✅ NO OVERLAP") << std::endl;
    std::cout << "  - Shifted Right vs Insert intersection: " << (overlap_ri ? "❌ OVERLAPPED" : "✅ NO OVERLAP") << std::endl;

    if (overlap_lr || overlap_li || overlap_ri) {
        std::cerr << "  ❌ FAIL: Swept volume extrusions overlap with one another." << std::endl;
        return;
    }

    std::cout << "  ✅ 3-Piece Swept Volume Demoldability Test Passed." << std::endl;
}

int main() {
    run_extrusion_overlap_test();
    return 0;
}
