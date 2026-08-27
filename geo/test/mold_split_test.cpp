#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;

typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef CGAL::Surface_mesh<IK::Point_3> Mesh;
typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
typedef CGAL::AABB_traits<IK, Primitive> Traits;
typedef CGAL::AABB_tree<Traits> Tree;

struct Point3D {
    double x, y, z;
};

// Project a point p radially from center c to the boundary of a box
Point3D project_to_box(Point3D p, Point3D c, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax) {
    double dx = p.x - c.x;
    double dy = p.y - c.y;
    double dz = p.z - c.z;

    double t = 999999.0;

    if (std::abs(dx) > 1e-9) {
        double tx = (dx > 0) ? (xmax - c.x) / dx : (xmin - c.x) / dx;
        if (tx > 0 && tx < t) t = tx;
    }
    if (std::abs(dy) > 1e-9) {
        double ty = (dy > 0) ? (ymax - c.y) / dy : (ymin - c.y) / dy;
        if (ty > 0 && ty < t) t = ty;
    }
    if (std::abs(dz) > 1e-9) {
        double tz = (dz > 0) ? (zmax - c.z) / dz : (zmin - c.z) / dz;
        if (tz > 0 && tz < t) t = tz;
    }

    return {c.x + t * dx, c.y + t * dy, c.z + t * dz};
}

void run_mold_split_test() {
    MockVFS vfs("mold_split");
    register_all_ops(&vfs);

    std::cout << "Running Mold Splitting Test on Bear..." << std::endl;

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
    
    // 2. Load and Repair the Bear Mesh
    std::cout << "  - Converting bear to ExactMesh and repairing..." << std::endl;
    ExactMesh mesh_bear = boolean::Engine::geometry_to_mesh(bear_geo);
    
    // CGAL Repairs
    CGAL::Polygon_mesh_processing::stitch_borders(mesh_bear);
    CGAL::Polygon_mesh_processing::remove_almost_degenerate_faces(mesh_bear);
    CGAL::Polygon_mesh_processing::remove_isolated_vertices(mesh_bear);

    // Close any open boundary holes using Euler boundary fan triangulation
    if (!CGAL::is_closed(mesh_bear)) {
        std::cout << "    * Bear has open holes, closing border cycles..." << std::endl;
        std::vector<ExactMesh::Halfedge_index> borders;
        for (auto h : mesh_bear.halfedges()) {
            if (mesh_bear.is_border(h)) borders.push_back(h);
        }
        for (auto h : borders) {
            if (mesh_bear.is_border(h)) {
                std::vector<ExactMesh::Vertex_index> hole_vs;
                auto curr = h;
                do {
                    hole_vs.push_back(mesh_bear.target(curr));
                    curr = mesh_bear.next(curr);
                } while (curr != h && hole_vs.size() < 1000);
                
                if (hole_vs.size() >= 3) {
                    auto v0 = hole_vs[0];
                    for (size_t i = 1; i + 1 < hole_vs.size(); ++i) {
                        mesh_bear.add_face(v0, hole_vs[i], hole_vs[i+1]);
                    }
                }
            }
        }
    }

    // Verify Manifoldness & Self-Intersections AFTER closing
    bool is_closed = CGAL::is_closed(mesh_bear);
    bool does_self_inter = CGAL::Polygon_mesh_processing::does_self_intersect(mesh_bear);
    bool is_valid = mesh_bear.is_valid();

    std::cout << "    * Post-repair Bear Mesh is_closed: " << (is_closed ? "YES (WATERTIGHT SOLID)" : "NO (FAIL)") << std::endl;
    std::cout << "    * Post-repair Bear Mesh does_self_intersect: " << (does_self_inter ? "YES (ERROR)" : "NO (CLEAN)") << std::endl;
    std::cout << "    * Post-repair Bear Mesh is_valid: " << (is_valid ? "YES" : "NO") << std::endl;

    if (!is_closed || does_self_inter) {
        std::cerr << "  ❌ FAIL: Repaired bear is not a clean watertight solid!" << std::endl;
        return;
    }

    // Update bear_geo with the repaired mesh
    bear_geo = boolean::Engine::mesh_to_geometry(mesh_bear);
    std::cout << "    * Repaired bear has " << bear_geo.triangles.size() << " triangles, " << bear_geo.vertices.size() << " vertices." << std::endl;

    // 3. Compute optimal direction and extract patch P1 for d1 = [-1, 0, 0]
    IK::Vector_3 dir_ik(-1.0, 0.0, 0.0);

    // Build Mesh and AABB Tree for global visibility
    Mesh mesh_ik = boolean::Engine::geometry_to_mesh_ik(bear_geo);
    Tree tree(CGAL::faces(mesh_ik).first, CGAL::faces(mesh_ik).second, mesh_ik);
    tree.build();

    // Identify visible facets for patch P1
    std::vector<int> p1_faces;
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        const auto& p0 = bear_geo.vertices[tri[0]];
        const auto& p1 = bear_geo.vertices[tri[1]];
        const auto& p2 = bear_geo.vertices[tri[2]];
        
        double cx = CGAL::to_double(p0.x + p1.x + p2.x) / 3.0;
        double cy = CGAL::to_double(p0.y + p1.y + p2.y) / 3.0;
        double cz = CGAL::to_double(p0.z + p1.z + p2.z) / 3.0;

        IK::Vector_3 u(CGAL::to_double(p1.x - p0.x), CGAL::to_double(p1.y - p0.y), CGAL::to_double(p1.z - p0.z));
        IK::Vector_3 v(CGAL::to_double(p2.x - p0.x), CGAL::to_double(p2.y - p0.y), CGAL::to_double(p2.z - p0.z));
        IK::Vector_3 n = CGAL::cross_product(u, v);
        double nlen = std::sqrt(CGAL::to_double(n.squared_length()));
        if (nlen > 1e-12) n = n / nlen;

        double dot = CGAL::to_double(n * dir_ik);
        if (dot < 0.0) continue; // local draft failure

        IK::Point_3 ray_origin(cx + n.x() * 1e-3, cy + n.y() * 1e-3, cz + n.z() * 1e-3);
        IK::Ray_3 ray(ray_origin, dir_ik);
        std::vector<typename Tree::Intersection_and_primitive_id<IK::Ray_3>::Type> inters;
        tree.all_intersections(ray, std::back_inserter(inters));
        bool blocked = false;
        for (const auto& inter : inters) {
            IK::Point_3 pt;
            if (const IK::Point_3* pi = std::get_if<IK::Point_3>(&inter.first)) pt = *pi;
            else if (const IK::Segment_3* ps = std::get_if<IK::Segment_3>(&inter.first)) pt = ps->source();
            else continue;
            if (CGAL::to_double(CGAL::squared_distance(pt, ray_origin)) > 1e-4) {
                blocked = true;
                break;
            }
        }
        if (!blocked) {
            p1_faces.push_back((int)f_idx);
        }
    }
    std::cout << "  - Extracted " << p1_faces.size() << " facets for Primary Patch P1 out of " << bear_geo.triangles.size() << " total." << std::endl;

    // 4. Extract 3D Parting Silhouette Line (Spine to Belly contour)
    std::map<std::pair<int, int>, std::vector<int>> edge_faces;
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        for (int i = 0; i < 3; ++i) {
            int u = std::min(tri[i], tri[(i + 1) % 3]);
            int v = std::max(tri[i], tri[(i + 1) % 3]);
            edge_faces[{u, v}].push_back((int)f_idx);
        }
    }

    auto get_face_normal = [&](int f_idx) -> IK::Vector_3 {
        const auto& tri = bear_geo.triangles[f_idx];
        const auto& p0 = bear_geo.vertices[tri[0]];
        const auto& p1 = bear_geo.vertices[tri[1]];
        const auto& p2 = bear_geo.vertices[tri[2]];
        IK::Vector_3 u(CGAL::to_double(p1.x - p0.x), CGAL::to_double(p1.y - p0.y), CGAL::to_double(p1.z - p0.z));
        IK::Vector_3 v(CGAL::to_double(p2.x - p0.x), CGAL::to_double(p2.y - p0.y), CGAL::to_double(p2.z - p0.z));
        return CGAL::cross_product(u, v);
    };

    std::vector<std::pair<int, int>> parting_segments;
    for (const auto& [edge, faces] : edge_faces) {
        if (faces.size() >= 2) {
            double dot0 = CGAL::to_double(get_face_normal(faces[0]) * dir_ik);
            double dot1 = CGAL::to_double(get_face_normal(faces[1]) * dir_ik);
            if ((dot0 >= 0 && dot1 < 0) || (dot0 < 0 && dot1 >= 0)) {
                parting_segments.push_back(edge);
            }
        }
    }
    std::cout << "  - Extracted " << parting_segments.size() << " 3D parting silhouette segments." << std::endl;

    // 4. Compute stock bounds in pure FT
    FT b_xmin = 1000, b_xmax = -1000;
    FT b_ymin = 1000, b_ymax = -1000;
    FT b_zmin = 1000, b_zmax = -1000;
    for (const auto& v : bear_geo.vertices) {
        if (v.x < b_xmin) b_xmin = v.x;
        if (v.x > b_xmax) b_xmax = v.x;
        if (v.y < b_ymin) b_ymin = v.y;
        if (v.y > b_ymax) b_ymax = v.y;
        if (v.z < b_zmin) b_zmin = v.z;
        if (v.z > b_zmax) b_zmax = v.z;
    }
    FT pad(10);
    FT xmin = b_xmin - pad, xmax = b_xmax + pad;
    FT ymin = b_ymin - pad, ymax = b_ymax + pad;
    FT zmin = b_zmin - pad, zmax = b_zmax + pad;

    // 5. Construct Solid Mold Stock Box in pure FT
    Geometry stock_geo;
    stock_geo.vertices.push_back({xmin, ymin, zmin}); // 0
    stock_geo.vertices.push_back({xmax, ymin, zmin}); // 1
    stock_geo.vertices.push_back({xmax, ymax, zmin}); // 2
    stock_geo.vertices.push_back({xmin, ymax, zmin}); // 3
    stock_geo.vertices.push_back({xmin, ymin, zmax}); // 4
    stock_geo.vertices.push_back({xmax, ymin, zmax}); // 5
    stock_geo.vertices.push_back({xmax, ymax, zmax}); // 6
    stock_geo.vertices.push_back({xmin, ymax, zmax}); // 7
    stock_geo.triangles.push_back({0, 2, 1}); stock_geo.triangles.push_back({0, 3, 2});
    stock_geo.triangles.push_back({4, 5, 6}); stock_geo.triangles.push_back({4, 6, 7});
    stock_geo.triangles.push_back({0, 1, 5}); stock_geo.triangles.push_back({0, 5, 4});
    stock_geo.triangles.push_back({1, 2, 6}); stock_geo.triangles.push_back({1, 6, 5});
    stock_geo.triangles.push_back({2, 3, 7}); stock_geo.triangles.push_back({2, 7, 6});
    stock_geo.triangles.push_back({3, 0, 4}); stock_geo.triangles.push_back({3, 4, 7});

    ExactMesh mesh_stock = boolean::Engine::geometry_to_mesh(stock_geo);

    // 6. Direct Left and Right Stock Blocks in pure FT
    Geometry left_half_geo;
    left_half_geo.vertices.push_back({xmin, ymin, zmin}); // 0
    left_half_geo.vertices.push_back({FT(0), ymin, zmin}); // 1
    left_half_geo.vertices.push_back({FT(0), ymax, zmin}); // 2
    left_half_geo.vertices.push_back({xmin, ymax, zmin}); // 3
    left_half_geo.vertices.push_back({xmin, ymin, zmax}); // 4
    left_half_geo.vertices.push_back({FT(0), ymin, zmax}); // 5
    left_half_geo.vertices.push_back({FT(0), ymax, zmax}); // 6
    left_half_geo.vertices.push_back({xmin, ymax, zmax}); // 7
    left_half_geo.triangles.push_back({0, 2, 1}); left_half_geo.triangles.push_back({0, 3, 2});
    left_half_geo.triangles.push_back({4, 5, 6}); left_half_geo.triangles.push_back({4, 6, 7});
    left_half_geo.triangles.push_back({0, 1, 5}); left_half_geo.triangles.push_back({0, 5, 4});
    left_half_geo.triangles.push_back({1, 2, 6}); left_half_geo.triangles.push_back({1, 6, 5});
    left_half_geo.triangles.push_back({2, 3, 7}); left_half_geo.triangles.push_back({2, 7, 6});
    left_half_geo.triangles.push_back({3, 0, 4}); left_half_geo.triangles.push_back({3, 4, 7});

    Geometry right_half_geo;
    right_half_geo.vertices.push_back({FT(0), ymin, zmin}); // 0
    right_half_geo.vertices.push_back({xmax, ymin, zmin}); // 1
    right_half_geo.vertices.push_back({xmax, ymax, zmin}); // 2
    right_half_geo.vertices.push_back({FT(0), ymax, zmin}); // 3
    right_half_geo.vertices.push_back({FT(0), ymin, zmax}); // 4
    right_half_geo.vertices.push_back({xmax, ymin, zmax}); // 5
    right_half_geo.vertices.push_back({xmax, ymax, zmax}); // 6
    right_half_geo.vertices.push_back({FT(0), ymax, zmax}); // 7
    right_half_geo.triangles.push_back({0, 2, 1}); right_half_geo.triangles.push_back({0, 3, 2});
    right_half_geo.triangles.push_back({4, 5, 6}); right_half_geo.triangles.push_back({4, 6, 7});
    right_half_geo.triangles.push_back({0, 1, 5}); right_half_geo.triangles.push_back({0, 5, 4});
    right_half_geo.triangles.push_back({1, 2, 6}); right_half_geo.triangles.push_back({1, 6, 5});
    right_half_geo.triangles.push_back({2, 3, 7}); right_half_geo.triangles.push_back({2, 7, 6});
    right_half_geo.triangles.push_back({3, 0, 4}); right_half_geo.triangles.push_back({3, 4, 7});

    ExactMesh mesh_left = boolean::Engine::geometry_to_mesh(left_half_geo);
    boolean::Engine::cut_mesh_by_mesh(mesh_left, mesh_bear);

    ExactMesh mesh_right = boolean::Engine::geometry_to_mesh(right_half_geo);
    boolean::Engine::cut_mesh_by_mesh(mesh_right, mesh_bear);

    // 7. Detect Trapped Undercut Faces (Chest Undercut)
    std::vector<int> trapped_faces;
    IK::Vector_3 avg_insert_normal(0, 0, 0);
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        IK::Vector_3 n = get_face_normal((int)f_idx);
        double nlen = std::sqrt(CGAL::to_double(n.squared_length()));
        if (nlen > 1e-12) n = n / nlen;
        double dot_l = CGAL::to_double(n * IK::Vector_3(-1, 0, 0));
        double dot_r = CGAL::to_double(n * IK::Vector_3(1, 0, 0));
        if (dot_l < 0.0 && dot_r < 0.0) {
            trapped_faces.push_back((int)f_idx);
            avg_insert_normal = avg_insert_normal + n;
        }
    }
    std::cout << "  - Detected " << trapped_faces.size() << " trapped undercut facets on chest." << std::endl;

    double ilen = std::sqrt(CGAL::to_double(avg_insert_normal.squared_length()));
    double idx = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.x()) / ilen : 0.274;
    double idy = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.y()) / ilen : -0.241;
    double idz = (ilen > 1e-12) ? CGAL::to_double(avg_insert_normal.z()) / ilen : -0.931;

    // 8. Build Insert Solid Geometry
    // Construct local bounding box covering trapped cavity floor and extending out to zmin
    double ins_xmin = -20.0, ins_xmax = 20.0;
    double ins_ymin = -60.0, ins_ymax = -10.0;
    double ins_zmin = CGAL::to_double(zmin) - 5.0, ins_zmax = 45.0;

    Geometry slide_box_geo;
    slide_box_geo.vertices.push_back({(FT)ins_xmin, (FT)ins_ymin, (FT)ins_zmin}); // 0
    slide_box_geo.vertices.push_back({(FT)ins_xmax, (FT)ins_ymin, (FT)ins_zmin}); // 1
    slide_box_geo.vertices.push_back({(FT)ins_xmax, (FT)ins_ymax, (FT)ins_zmin}); // 2
    slide_box_geo.vertices.push_back({(FT)ins_xmin, (FT)ins_ymax, (FT)ins_zmin}); // 3
    slide_box_geo.vertices.push_back({(FT)ins_xmin, (FT)ins_ymin, (FT)ins_zmax}); // 4
    slide_box_geo.vertices.push_back({(FT)ins_xmax, (FT)ins_ymin, (FT)ins_zmax}); // 5
    slide_box_geo.vertices.push_back({(FT)ins_xmax, (FT)ins_ymax, (FT)ins_zmax}); // 6
    slide_box_geo.vertices.push_back({(FT)ins_xmin, (FT)ins_ymax, (FT)ins_zmax}); // 7
    slide_box_geo.triangles.push_back({0, 2, 1}); slide_box_geo.triangles.push_back({0, 3, 2});
    slide_box_geo.triangles.push_back({4, 5, 6}); slide_box_geo.triangles.push_back({4, 6, 7});
    slide_box_geo.triangles.push_back({0, 1, 5}); slide_box_geo.triangles.push_back({0, 5, 4});
    slide_box_geo.triangles.push_back({1, 2, 6}); slide_box_geo.triangles.push_back({1, 6, 5});
    slide_box_geo.triangles.push_back({2, 3, 7}); slide_box_geo.triangles.push_back({2, 7, 6});
    slide_box_geo.triangles.push_back({3, 0, 4}); slide_box_geo.triangles.push_back({3, 4, 7});

    ExactMesh mesh_slide_box = boolean::Engine::geometry_to_mesh(slide_box_geo);

    // Carve 3 Solid Pieces
    ExactMesh mesh_insert = mesh_slide_box;
    boolean::Engine::cut_mesh_by_mesh(mesh_insert, mesh_bear);

    ExactMesh mesh_left_final = mesh_left;
    boolean::Engine::cut_mesh_by_mesh(mesh_left_final, mesh_slide_box);

    ExactMesh mesh_right_final = mesh_right;
    boolean::Engine::cut_mesh_by_mesh(mesh_right_final, mesh_slide_box);

    double vol_stock = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh_stock));
    double vol_left = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh_left_final));
    double vol_right = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh_right_final));
    double vol_insert = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(mesh_insert));

    std::cout << "  - Stock Mold Box Volume: " << vol_stock << " mm^3" << std::endl;
    std::cout << "  - Left Block (M1) Volume: " << vol_left << " mm^3 (" << (vol_left / vol_stock * 100.0) << "% of mold)" << std::endl;
    std::cout << "  - Right Block (M2) Volume: " << vol_right << " mm^3 (" << (vol_right / vol_stock * 100.0) << "% of mold)" << std::endl;
    std::cout << "  - Insert Block (M3) Volume: " << vol_insert << " mm^3 (" << (vol_insert / vol_stock * 100.0) << "% of mold)" << std::endl;
    std::cout << "  - Total 3-Piece Solid Partitioned: " << (vol_left + vol_right + vol_insert) << " mm^3 (" << ((vol_left + vol_right + vol_insert) / vol_stock * 100.0) << "% of mold)" << std::endl;

    // 9. Render 3-Piece Exploded Solid Decomposition
    Geometry geo_m1 = boolean::Engine::mesh_to_geometry(mesh_left_final);
    Geometry geo_m2 = boolean::Engine::mesh_to_geometry(mesh_right_final);
    Geometry geo_m3 = boolean::Engine::mesh_to_geometry(mesh_insert);

    Shape piece1_shape = JotVfsProtocol::make_shape(&vfs, geo_m1, {{"color", "#2bee2b"}, {"name", "piece1_left"}});
    Shape piece2_shape = JotVfsProtocol::make_shape(&vfs, geo_m2, {{"color", "#ee2b2b"}, {"name", "piece2_right"}});
    Shape piece3_shape = JotVfsProtocol::make_shape(&vfs, geo_m3, {{"color", "#2b2bee"}, {"name", "piece3_insert"}});
    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, {{"color", "#ffffff"}, {"name", "bear"}});

    piece1_shape.tf = Matrix::translate(-40.0, 0.0, 0.0);
    piece2_shape.tf = Matrix::translate(40.0, 0.0, 0.0);
    piece3_shape.tf = Matrix::translate(0.0, 0.0, -40.0);

    Shape composite;
    composite.components.push_back(piece1_shape);
    composite.components.push_back(piece2_shape);
    composite.components.push_back(piece3_shape);
    composite.components.push_back(bear_shape);

    composite.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

    std::cout << "  - Rendering 3-Piece Solid Mold Decomposition (Isometric)..." << std::endl;
    auto png_data = Rasterizer::render_png(&vfs, composite, 1024, 1024, -0.61547, 0.78539);
    if (!png_data.empty()) {
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/bear_mold_3piece_solid.png", std::ios::binary);
        out.write((const char*)png_data.data(), png_data.size());
        std::cout << "  📸 Saved actual/bear_mold_3piece_solid.png" << std::endl;
    }

    std::cout << "  ✅ 3-Piece Solid Mold Decomposition Test Completed Successfully." << std::endl;
}

int main() {
    run_mold_split_test();
    return 0;
}
