#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "fuse_op.h"
#include "mold/harmonic.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

inline ExactMesh soup_to_solid(std::vector<EK::Point_3>& soup_points, std::vector<std::vector<size_t>>& soup_polygons) {
    fix::repair_solid_soup(soup_points, soup_polygons);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(soup_points, soup_polygons);
    ExactMesh mesh;
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(soup_points, soup_polygons, mesh);
    CGAL::Polygon_mesh_processing::stitch_borders(mesh);
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    mesh.collect_garbage();
    if (CGAL::is_closed(mesh)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(mesh);
    }
    return mesh;
}

inline ExactMesh get_shape_mesh(const Shape& in, MockVFS& vfs) {
    ExactMesh mesh;
    in.walk([&](const Shape& node) {
        if (node.geometry.has_value() && node.is_real() && !node.is_ghost()) {
            Geometry geo = vfs.read<Geometry>(*node.geometry);
            ExactMesh comp = boolean::Engine::geometry_to_mesh(geo);
            boolean::Engine::transform_mesh(comp, node.tf);
            if (mesh.is_empty()) {
                mesh = std::move(comp);
            } else {
                boolean::Engine::join_mesh_by_mesh(mesh, comp);
            }
        }
    });
    mesh.collect_garbage();
    return mesh;
}

struct PatchBoundaryExtraction {
    std::vector<EK::Point_3> floor_soup_points;
    std::vector<std::vector<size_t>> floor_soup_polygons;
    std::vector<BoundarySegment3D> inner_segments;
};

inline PatchBoundaryExtraction extract_patch_and_boundaries(
    const ExactMesh& mesh,
    const std::function<bool(const EK::Vector_3&)>& face_selector
) {
    PatchBoundaryExtraction result;
    std::set<ExactMesh::Face_index> patch_faces;

    for (auto f : mesh.faces()) {
        auto h = mesh.halfedge(f);
        auto p0 = mesh.point(mesh.source(h));
        auto p1 = mesh.point(mesh.target(h));
        auto p2 = mesh.point(mesh.target(mesh.next(h)));
        EK::Vector_3 n = CGAL::normal(p0, p1, p2);
        if (face_selector(n)) {
            patch_faces.insert(f);
            size_t idx = result.floor_soup_points.size();
            result.floor_soup_points.push_back(p0);
            result.floor_soup_points.push_back(p2); // CW winding for downward outward normal
            result.floor_soup_points.push_back(p1);
            result.floor_soup_polygons.push_back({idx, idx + 1, idx + 2});
        }
    }

    for (auto f : patch_faces) {
        auto h = mesh.halfedge(f);
        auto h_start = h;
        do {
            auto h_opp = mesh.opposite(h);
            bool is_patch_border = false;
            if (h_opp == ExactMesh::null_halfedge()) {
                is_patch_border = true;
            } else {
                auto f_opp = mesh.face(h_opp);
                if (f_opp == ExactMesh::null_face() || !patch_faces.count(f_opp)) {
                    is_patch_border = true;
                }
            }
            if (is_patch_border) {
                auto p_src = mesh.point(mesh.source(h));
                auto p_tgt = mesh.point(mesh.target(h));
                result.inner_segments.push_back({
                    CDT_Kernel::Point_2(p_src.x(), p_src.y()),
                    CDT_Kernel::Point_2(p_tgt.x(), p_tgt.y()),
                    p_src.z(),
                    p_tgt.z()
                });
            }
            h = mesh.next(h);
        } while (h != h_start);
    }
    return result;
}

int main() {
    MockVFS vfs("mold_harmonic_test");
    register_all_ops(&vfs);

    std::cout << "Testing Harmonic Minimal Surface Parting Engine via CAD Models..." << std::endl;

    // =========================================================================
    // Test 1: Flat Cube (Box 10x10x10)
    // =========================================================================
    std::cout << "  - Test 1: Flat Cube (Box 10x10x10)..." << std::endl;
    {
        fs::Selector box_sel("jot/Box");
        box_sel.parameters["width"] = 10.0;
        box_sel.parameters["height"] = 10.0;
        box_sel.parameters["depth"] = 10.0;
        Shape cube_shape = vfs.read<Shape>(box_sel.with_output("$out"));
        ExactMesh cube_mesh = get_shape_mesh(cube_shape, vfs);

        auto patch = extract_patch_and_boundaries(cube_mesh, [](const EK::Vector_3& n) {
            return n.z() > FT(0);
        });

        HarmonicStockParams stock;
        stock.u_min = FT(-10);
        stock.u_max = FT(10);
        stock.v_min = FT(-10);
        stock.v_max = FT(10);
        stock.w_top = FT(10);
        stock.max_edge_len = 2.0;

        ExactMesh piece1 = construct_harmonic_wedge(patch.inner_segments, stock);

        assert(CGAL::is_closed(piece1) && "Cube piece 1 must be a closed watertight 2-manifold!");
        double vol1 = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(piece1));
        std::cout << "    - Piece 1 volume: " << vol1 << " mm^3 (expected 2000 mm^3)" << std::endl;
        assert(std::abs(vol1 - 2000.0) < 1e-4 && "Cube Piece 1 volume must be exactly 2000 mm^3!");
    }

    // =========================================================================
    // Test 2: Concave L-Bracket (Re-entrant Inside Corner)
    // =========================================================================
    std::cout << "  - Test 2: Concave L-Bracket (Re-entrant Inside Corner)..." << std::endl;
    {
        fs::Selector block_sel("jot/Box");
        block_sel.parameters["width"] = 20.0;
        block_sel.parameters["height"] = 20.0;
        block_sel.parameters["depth"] = 10.0;
        Shape block_shape = vfs.read<Shape>(block_sel.with_output("$out"));

        fs::Selector notch_sel("jot/Box");
        notch_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
        notch_sel.parameters["height"] = nlohmann::json::array({0.0, 11.0});
        notch_sel.parameters["depth"] = nlohmann::json::array({-6.0, 6.0});
        Shape notch_shape = vfs.read<Shape>(notch_sel.with_output("$out"));

        fs::Selector cut_sel("jot/cut");
        cut_sel.parameters["$in"] = block_shape.to_json();
        cut_sel.parameters["tools"] = nlohmann::json::array({notch_shape.to_json()});
        Shape l_shape = vfs.read<Shape>(cut_sel.with_output("$out"));
        ExactMesh l_mesh = get_shape_mesh(l_shape, vfs);

        auto patch = extract_patch_and_boundaries(l_mesh, [](const EK::Vector_3& n) {
            return n.z() > FT(0);
        });

        HarmonicStockParams stock;
        stock.u_min = FT(-15);
        stock.u_max = FT(15);
        stock.v_min = FT(-15);
        stock.v_max = FT(15);
        stock.w_top = FT(10);
        stock.max_edge_len = 3.0;

        ExactMesh l_piece = construct_harmonic_wedge(patch.inner_segments, stock);

        assert(CGAL::is_closed(l_piece) && "L-bracket piece must be a closed watertight 2-manifold!");
        double vol_l = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(l_piece));
        std::cout << "    - L-Bracket Piece 1 volume: " << vol_l << " mm^3 (expected 4500 mm^3)" << std::endl;
        assert(std::abs(vol_l - 4500.0) < 1e-4 && "L-Bracket Piece 1 volume must be exactly 30x30x5 = 4500 mm^3!");
    }

    // =========================================================================
    // Test 3: T-Bracket (Two Symmetrical Re-entrant Inside Corners)
    // =========================================================================
    std::cout << "  - Test 3: T-Bracket (Two Symmetrical Re-entrant Inside Corners)..." << std::endl;
    {
        fs::Selector t_bar_sel = fs::Selector{"jot/Box", {{"width", 30.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
        fs::Selector t_stem_sel = fs::Selector{"jot/Box", {
            {"width", nlohmann::json::array({-5.0, 5.0})},
            {"height", nlohmann::json::array({5.0, 25.0})},
            {"depth", nlohmann::json::array({-5.0, 5.0})}
        }}.with_output("$out");
        Shape t_bar = vfs.read<Shape>(t_bar_sel);
        Shape t_stem = vfs.read<Shape>(t_stem_sel);
        fs::Selector t_fuse_sel = fs::Selector{"jot/Fuse", {{"shapes", {vfs.materialize(t_bar), vfs.materialize(t_stem)}}}}.with_output("$out");
        FusePrimitiveOp<>::execute(&vfs, t_fuse_sel, {t_bar, t_stem});
        Shape t_shape = vfs.read<Shape>(t_fuse_sel);
        ExactMesh t_mesh = get_shape_mesh(t_shape, vfs);

        auto patch = extract_patch_and_boundaries(t_mesh, [](const EK::Vector_3& n) {
            return n.z() > FT(0);
        });

        HarmonicStockParams stock;
        stock.u_min = FT(-20);
        stock.u_max = FT(20);
        stock.v_min = FT(-10);
        stock.v_max = FT(30);
        stock.w_top = FT(10);
        stock.max_edge_len = 4.0;

        ExactMesh t_piece = construct_harmonic_wedge(patch.inner_segments, stock);

        assert(CGAL::is_closed(t_piece) && "T-bracket piece must be a closed watertight 2-manifold!");
        double vol_t = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(t_piece));
        std::cout << "    - T-Bracket Piece 1 volume: " << vol_t << " mm^3 (expected 8000 mm^3)" << std::endl;
        assert(std::abs(vol_t - 8000.0) < 1e-4 && "T-Bracket Piece 1 volume must be exactly 40x40x5 = 8000 mm^3!");
    }

    // =========================================================================
    // Test 4: 2-Way Planar Cross (4 Inside Re-entrant Corners)
    // =========================================================================
    std::cout << "  - Test 4: 2-Way Planar Cross (4 Inside Re-entrant Corners)..." << std::endl;
    {
        fs::Selector pboxX_sel = fs::Selector{"jot/Box", {{"width", 30.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
        fs::Selector pboxY_sel = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 30.0}, {"depth", 10.0}}}.with_output("$out");
        Shape psX = vfs.read<Shape>(pboxX_sel);
        Shape psY = vfs.read<Shape>(pboxY_sel);
        fs::Selector pcross_sel = fs::Selector{"jot/Fuse", {{"shapes", {vfs.materialize(psX), vfs.materialize(psY)}}}}.with_output("$out");
        FusePrimitiveOp<>::execute(&vfs, pcross_sel, {psX, psY});
        Shape cross_shape = vfs.read<Shape>(pcross_sel);
        ExactMesh cross_mesh = get_shape_mesh(cross_shape, vfs);

        auto patch = extract_patch_and_boundaries(cross_mesh, [](const EK::Vector_3& n) {
            return n.z() > FT(0);
        });

        HarmonicStockParams stock;
        stock.u_min = FT(-20);
        stock.u_max = FT(20);
        stock.v_min = FT(-20);
        stock.v_max = FT(20);
        stock.w_top = FT(10);
        stock.max_edge_len = 4.0;

        ExactMesh cross_piece = construct_harmonic_wedge(patch.inner_segments, stock);

        assert(CGAL::is_closed(cross_piece) && "Planar cross piece must be a closed watertight 2-manifold!");
        double vol_cross = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(cross_piece));
        std::cout << "    - Planar Cross Piece 1 volume: " << vol_cross << " mm^3 (expected 8000 mm^3)" << std::endl;
        assert(std::abs(vol_cross - 8000.0) < 1e-4 && "Planar Cross Piece 1 volume must be exactly 40x40x5 = 8000 mm^3!");
    }

    std::cout << "✅ All 4 harmonic minimal parting tests passed with exact volume conservation!" << std::endl;
    return 0;
}
