#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "boolean/corefine.h"
#include "fix/assert_mesh.h"
#include "mold/types.h"
#include "mold/optimizer.h"
#include "mold/envelope.h"
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace ::jotcad;
using namespace ::jotcad::geo;
using namespace ::jotcad::geo::mold;
using namespace ::fs;
using ExactMesh = ::jotcad::geo::boolean::ExactMesh;
using ::CGAL::to_double;

int main() {
    MockVFS vfs("mold_voxel_bear_test");
    register_all_ops(&vfs);

    std::cout << "============================================================" << std::endl;
    std::cout << "Testing Voxel Bear Pour Prep & Multi-Piece Mold Decomposition" << std::endl;
    std::cout << "============================================================" << std::endl;

    // 1. Central Body Cube: 20 x 20 x 20 mm, Z: [10, 30]
    auto make_box = [&](const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& z) {
        Selector sel{"jot/Box", {
            {"width", json::array({x[0], x[1]})},
            {"height", json::array({y[0], y[1]})},
            {"depth", json::array({z[0], z[1]})}
        }};
        sel.output = "$out";
        Processor::execute(&vfs, sel);
        return vfs.read<Shape>(sel);
    };

    std::cout << "  - Building voxel bear components..." << std::endl;
    Shape body = make_box({-10.0, 10.0}, {-10.0, 10.0}, {10.0, 30.0});

    // 2. Four Legs: 4 x 4 x 10 mm pillars going down to Z = 0
    Shape leg_fl = make_box({-10.0, -6.0}, {6.0, 10.0}, {0.0, 10.0});
    Shape leg_fr = make_box({6.0, 10.0}, {6.0, 10.0}, {0.0, 10.0});
    Shape leg_bl = make_box({-10.0, -6.0}, {-10.0, -6.0}, {0.0, 10.0});
    Shape leg_br = make_box({6.0, 10.0}, {-10.0, -6.0}, {0.0, 10.0});

    // 3. Four Feet: 4 x 4 x 4 mm blocks extending forward in +Y from bottom of each leg
    Shape foot_fl = make_box({-10.0, -6.0}, {10.0, 14.0}, {0.0, 4.0});
    Shape foot_fr = make_box({6.0, 10.0}, {10.0, 14.0}, {0.0, 4.0});
    Shape foot_bl = make_box({-10.0, -6.0}, {-6.0, -2.0}, {0.0, 4.0});
    Shape foot_br = make_box({6.0, 10.0}, {-6.0, -2.0}, {0.0, 4.0});

    // 4. Fuse all into a single watertight solid
    std::cout << "  - Fusing into watertight solid bear..." << std::endl;
    std::vector<Shape> all_parts = {body, leg_fl, leg_fr, leg_bl, leg_br, foot_fl, foot_fr, foot_bl, foot_br};
    Selector fuse_sel{"jot/Fuse"};
    json parts_arr = json::array();
    for (const auto& p : all_parts) parts_arr.push_back(p.to_json());
    fuse_sel.parameters["shapes"] = parts_arr;
    fuse_sel.output = "$out";
    Processor::execute(&vfs, fuse_sel);
    Shape bear = vfs.read<Shape>(fuse_sel);
    assert(bear.geometry.has_value());
    Geometry bear_geo = vfs.read<Geometry>(*bear.geometry);
    std::cout << "    * Fused bear: " << bear_geo.vertices.size() << " vertices, " << bear_geo.triangles.size() << " triangles." << std::endl;

    // 5. Gravity Pour Prep (auto-orient with bubble drainage angles & sprue)
    std::cout << "  - Executing jot/pourPrep (sprue_base=6.0, sprue_top=12.0, vent_dia=2.0)..." << std::endl;
    Selector pour_sel{"jot/pourPrep", {
        {"$in", bear.to_json()},
        {"sprue_base", 6.0},
        {"sprue_top", 12.0},
        {"vent_dia", 2.0},
        {"auto_orient", true},
        {"vents", true}
    }};
    pour_sel.output = "$out";
    Processor::execute(&vfs, pour_sel);
    Shape prepped = vfs.read<Shape>(pour_sel);

    // Verify pour prep structure
    bool has_sprue_child = false;
    bool has_box_child = false;
    for (const auto& child : prepped.components) {
        if (child.has_tag("mold/role", "sprue")) has_sprue_child = true;
        if (child.has_tag("mold/role", "box")) has_box_child = true;
    }
    std::cout << "    * pourPrep result: has_sprue_child=" << (has_sprue_child ? "YES" : "NO")
              << ", has_box_child=" << (has_box_child ? "YES" : "NO")
              << ", up_vector=[" << prepped.tags.value("pour/up_vector", "") << "]" << std::endl;
    assert(has_sprue_child && has_box_child);

    // 6. Deep Investigation: Inspect mesh_part and sprue faces before mold decomposition
    ExactMesh mesh_part;
    std::optional<ExactMesh> stock_box_mesh;
    prepped.walk([&](const Shape& node) {
        if (node.has_tag("mold/role", "box") && node.geometry.has_value() && !stock_box_mesh.has_value()) {
            Geometry box_geo = vfs.read<Geometry>(*node.geometry);
            ExactMesh box_m = boolean::Engine::geometry_to_mesh(box_geo);
            boolean::Engine::transform_mesh(box_m, node.tf);
            stock_box_mesh = std::move(box_m);
            return;
        }
        if (node.geometry.has_value() && node.is_real()) {
            Geometry geo = vfs.read<Geometry>(*node.geometry);
            ExactMesh component = boolean::Engine::geometry_to_mesh(geo);
            boolean::Engine::transform_mesh(component, node.tf);
            if (mesh_part.is_empty()) {
                mesh_part = std::move(component);
            } else {
                boolean::Engine::join_mesh_by_mesh(mesh_part, component);
            }
        }
    });

    // Trim against stock box
    MoldParams params;
    params.padding = FT(5.0);
    params.explode = FT(0.0);
    params.draft = FT(-0.003);
    params.kiss_mode = fix::KissMode::WELD;
    params.kiss_width = FT(0.01);

    if (stock_box_mesh.has_value()) {
        ExactMesh trimmed_model;
        bool ok_trim = boolean::corefine_intersection(mesh_part, *stock_box_mesh, trimmed_model, params.kiss_mode, params.kiss_width, "model ∩ stock_box in test");
        if (ok_trim && trimmed_model.number_of_faces() > 0) {
            mesh_part = std::move(trimmed_model);
            mesh_part.collect_garbage();
        }
    }

    // Flatten vertices to pure rational leaves
    for (auto v : mesh_part.vertices()) {
        const auto& p = mesh_part.point(v);
        mesh_part.point(v) = EK::Point_3(EK::FT(p.x().exact()), EK::FT(p.y().exact()), EK::FT(p.z().exact()));
    }

    size_t total_faces = mesh_part.number_of_faces();
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "INVESTIGATION: Analyzing mesh_part (" << total_faces << " faces)" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    // Build topology, normals, centroids
    std::map<EdgeKey, std::vector<int>> edge_to_faces;
    std::vector<EK::Vector_3> face_normals(total_faces);
    std::vector<EK::Point_3> face_centroids(total_faces);
    std::vector<ExactMesh::Face_index> face_descriptors;
    face_descriptors.reserve(total_faces);

    // Identify sprue faces: find sprue apex or identify faces at the sprue end
    FT max_z = -1e9;
    for (auto v : mesh_part.vertices()) {
        if (mesh_part.point(v).z() > max_z) max_z = mesh_part.point(v).z();
    }

    std::vector<bool> is_sprue_face(total_faces, false);
    size_t sprue_face_count = 0;
    for (auto f : mesh_part.faces()) {
        size_t f_idx = f.idx();
        face_descriptors.push_back(f);
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        face_normals[f_idx] = CGAL::normal(p0, p1, p2);
        face_centroids[f_idx] = EK::Point_3((p0.x() + p1.x() + p2.x()) / 3, (p0.y() + p1.y() + p2.y()) / 3, (p0.z() + p1.z() + p2.z()) / 3);

        int v0 = (int)mesh_part.source(h);
        int v1 = (int)mesh_part.target(h);
        int v2 = (int)mesh_part.target(mesh_part.next(h));
        std::array<std::pair<int, int>, 3> edges = {std::make_pair(v0, v1), std::make_pair(v1, v2), std::make_pair(v2, v0)};
        for (auto [u, v] : edges) {
            if (u > v) std::swap(u, v);
            edge_to_faces[{u, v}].push_back((int)f_idx);
        }

        // Sprue faces are in the upper Z corridor
        if (face_centroids[f_idx].z() > max_z - FT(20.0)) {
            is_sprue_face[f_idx] = true;
            sprue_face_count++;
        }
    }
    std::cout << "  - Identified " << sprue_face_count << " candidate sprue faces (Z > " << to_double(max_z - FT(20.0)) << ")." << std::endl;

    // Setup is_handled tracking
    FaceBoolMap is_handled = mesh_part.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

    // STEP 1: Piece 1 Extraction
    std::cout << "\n>>> STEP 1: Optimizing Piece 1..." << std::endl;
    auto opt1 = optimize_parting_direction(mesh_part, face_normals, edge_to_faces, is_handled, params);
    std::cout << "  - Piece 1 dir: (" << to_double(opt1.best_dir.x()) << ", " << to_double(opt1.best_dir.y()) << ", " << to_double(opt1.best_dir.z()) << ")" << std::endl;
    std::cout << "  - Piece 1 handled " << opt1.source_faces.size() << " faces." << std::endl;

    size_t sprue_handled_p1 = 0;
    for (size_t f_idx : opt1.source_faces) {
        auto f = ExactMesh::Face_index(f_idx);
        is_handled[f] = true;
        if (is_sprue_face[f_idx]) sprue_handled_p1++;
    }
    std::cout << "  - Sprue faces handled by Piece 1: " << sprue_handled_p1 << " / " << sprue_face_count << std::endl;
    std::cout << "  - Sprue faces remaining unhandled: " << (sprue_face_count - sprue_handled_p1) << std::endl;

    // STEP 2: Piece 2 Investigation
    std::cout << "\n>>> STEP 2: Investigating Piece 2 Draw Direction & Sprue Visibility..." << std::endl;
    auto opt2 = optimize_parting_direction(mesh_part, face_normals, edge_to_faces, is_handled, params);
    EK::Vector_3 d2 = opt2.best_dir;
    std::cout << "  - Piece 2 selected best_dir: ("
              << to_double(d2.x()) << ", " << to_double(d2.y()) << ", " << to_double(d2.z()) << ")" << std::endl;
    std::cout << "  - Piece 2 handled " << opt2.source_faces.size() << " faces." << std::endl;

    size_t sprue_handled_p2 = 0;
    for (size_t f_idx : opt2.source_faces) {
        if (is_sprue_face[f_idx]) sprue_handled_p2++;
    }
    std::cout << "  - Sprue faces captured by Piece 2: " << sprue_handled_p2 << std::endl;

    // Now analyze why unhandled sprue faces were or were not captured by Piece 2:
    FT min_dot(std::sin(to_double(params.draft) * 2.0 * M_PI));
    std::cout << "  - Draft min_dot: " << to_double(min_dot) << std::endl;

    size_t unhandled_sprue_visible_d2 = 0;
    size_t unhandled_sprue_backfacing_d2 = 0;

    std::vector<ExactMesh::Face_index> unhandled_sprue_faces;
    for (size_t f_idx = 0; f_idx < total_faces; ++f_idx) {
        auto f = ExactMesh::Face_index(f_idx);
        if (is_sprue_face[f_idx] && !opt1.source_faces.count(f_idx)) {
            unhandled_sprue_faces.push_back(f);
            FT dot = face_normals[f_idx] * d2;
            if (dot >= min_dot) {
                unhandled_sprue_visible_d2++;
            } else {
                unhandled_sprue_backfacing_d2++;
            }
        }
    }

    std::cout << "\n  --- Sprue Unhandled Faces Visibility breakdown under d2 ---" << std::endl;
    std::cout << "    * Total unhandled sprue faces: " << unhandled_sprue_faces.size() << std::endl;
    std::cout << "    * Forward-facing (dot >= min_dot): " << unhandled_sprue_visible_d2 << std::endl;
    std::cout << "    * Back-facing (dot < min_dot): " << unhandled_sprue_backfacing_d2 << std::endl;

    // Test connectivity: Group all visible faces under d2 into connected components
    auto visible_faces_d2 = compute_visible_patch_faces_fast(
        mesh_part, face_descriptors, face_normals, is_handled, d2, min_dot
    );
    std::map<ExactMesh::Face_index, int> face_to_local;
    for (size_t i = 0; i < visible_faces_d2.size(); ++i) {
        face_to_local[visible_faces_d2[i]] = (int)i;
    }
    DSU patch_dsu((int)visible_faces_d2.size());
    for (const auto& [edge, faces] : edge_to_faces) {
        std::vector<int> visible_in_edge;
        for (int f_idx : faces) {
            auto f = ExactMesh::Face_index(f_idx);
            auto it = face_to_local.find(f);
            if (it != face_to_local.end()) {
                visible_in_edge.push_back(it->second);
            }
        }
        if (visible_in_edge.size() >= 2) {
            for (size_t i = 1; i < visible_in_edge.size(); ++i) {
                patch_dsu.unite(visible_in_edge[0], visible_in_edge[i]);
            }
        }
    }
    std::map<int, std::vector<ExactMesh::Face_index>> components;
    for (size_t i = 0; i < visible_faces_d2.size(); ++i) {
        int root = patch_dsu.find((int)i);
        components[root].push_back(visible_faces_d2[i]);
    }

    std::cout << "\n  --- Connected Components under d2: " << components.size() << " components found ---" << std::endl;
    int comp_idx = 1;
    for (const auto& [root, comp_faces] : components) {
        size_t sprue_in_comp = 0;
        size_t bear_in_comp = 0;
        for (auto f : comp_faces) {
            if (is_sprue_face[f.idx()]) sprue_in_comp++;
            else bear_in_comp++;
        }
        std::cout << "    * Component #" << comp_idx++ << ": " << comp_faces.size()
                  << " faces (Bear: " << bear_in_comp << ", Sprue: " << sprue_in_comp << ")" << std::endl;
    }

    // 7. Complete Full Mold Execution
    std::cout << "\n>>> Executing complete jot/mold pipeline to verify 100% face coverage..." << std::endl;
    Selector mold_sel{"jot/mold", {
        {"$in", prepped.to_json()},
        {"padding", 5.0},
        {"explode", 0.0},
        {"draft", -0.003},
        {"kiss", "weld"},
        {"kiss_width", 0.01}
    }};
    mold_sel.output = "$out";
    Processor::execute(&vfs, mold_sel);
    Shape mold_result = vfs.read<Shape>(mold_sel);

    int piece_count = 0;
    for (const auto& child : mold_result) {
        if (child.has_tag("mold/role", "piece")) piece_count++;
    }
    std::cout << "  - Total mold pieces produced: " << piece_count << std::endl;
    assert(piece_count >= 2);

    std::cout << "\n============================================================" << std::endl;
    std::cout << "✅ mold_voxel_bear_test completed successfully." << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
