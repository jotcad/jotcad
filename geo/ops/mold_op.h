#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "mold/types.h"
#include "mold/repair.h"
#include "mold/optimizer.h"
#include "mold/visibility.h"
#include "mold/ribbon.h"
#include "mold/partition.h"
#include "mold/verify.h"
#include <iostream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct MoldOp : P {
    static constexpr const char* path = "jot/mold";

    static void execute(
        fs::VFSNode* vfs,
        const fs::Selector& fulfilling,
        const Shape& in,
        double padding_val = 10.0,
        double explode_val = 0.0,
        double draft_val = 0.0
    ) {
        mold::MoldParams params;
        params.padding = FT(padding_val);
        params.explode = FT(explode_val);
        params.draft = FT(draft_val);

        // 1. Recursive Geometry Aggregation across Scene Graph
        Geometry world_geo;
        mold::collect_world_geometry_recursive(vfs, in, Matrix::identity(), world_geo);
        if (world_geo.vertices.empty() || world_geo.triangles.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 2. Exact Normalization and Watertight Mesh Repair
        mold::ExactMesh mesh_part = mold::normalize_and_repair_solid(world_geo);

        // 3. Topology & Geometric Centroids / Normals in Pure FT directly from mesh_part
        std::map<mold::EdgeKey, std::vector<int>> edge_to_faces;
        std::vector<EK::Vector_3> face_normals;
        std::vector<EK::Point_3> face_centroids;

        FT b_xmin = 1000000, b_xmax = -1000000;
        FT b_ymin = 1000000, b_ymax = -1000000;
        FT b_zmin = 1000000, b_zmax = -1000000;

        for (auto v : mesh_part.vertices()) {
            auto p = mesh_part.point(v);
            if (p.x() < b_xmin) b_xmin = p.x();
            if (p.x() > b_xmax) b_xmax = p.x();
            if (p.y() < b_ymin) b_ymin = p.y();
            if (p.y() > b_ymax) b_ymax = p.y();
            if (p.z() < b_zmin) b_zmin = p.z();
            if (p.z() > b_zmax) b_zmax = p.z();
        }

        int f_idx = 0;
        for (auto f : mesh_part.faces()) {
            auto h = mesh_part.halfedge(f);
            int v0 = (int)mesh_part.source(h);
            int v1 = (int)mesh_part.target(h);
            int v2 = (int)mesh_part.target(mesh_part.next(h));

            auto p0 = mesh_part.point(mesh_part.source(h));
            auto p1 = mesh_part.point(mesh_part.target(h));
            auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));

            face_normals.push_back(CGAL::normal(p0, p1, p2));
            face_centroids.push_back(EK::Point_3((p0.x() + p1.x() + p2.x()) / FT(3), (p0.y() + p1.y() + p2.y()) / FT(3), (p0.z() + p1.z() + p2.z()) / FT(3)));

            std::array<std::pair<int, int>, 3> edges = {std::make_pair(v0, v1), std::make_pair(v1, v2), std::make_pair(v2, v0)};
            for (auto [u, v] : edges) {
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back(f_idx);
            }
            f_idx++;
        }

        // 4. Stage 1: Large Conservative Stock Envelope for Unconstrained Extraction
        FT max_r_sq = 0;
        for (auto v : mesh_part.vertices()) {
            auto p = mesh_part.point(v);
            FT r2 = p.x()*p.x() + p.y()*p.y() + p.z()*p.z();
            if (r2 > max_r_sq) max_r_sq = r2;
        }
        double r_sphere = std::sqrt(CGAL::to_double(max_r_sq)) + CGAL::to_double(params.padding) + 100.0;
        FT R = FT(r_sphere);
        Geometry conservative_stock_geo = mold::build_box_geo(-R, R, -R, R, -R, R);
        mold::ExactMesh conservative_stock = boolean::Engine::geometry_to_mesh(conservative_stock_geo);

        // 5. Multi-Piece Mold Decomposition Loop
        mold::FaceBoolMap is_handled = mesh_part.add_property_map<mold::ExactMesh::Face_index, bool>("f:is_handled", false).first;
        size_t total_faces = mesh_part.number_of_faces();
        size_t handled_faces_count = 0;

        std::vector<mold::MoldPiece> mold_pieces;
        std::vector<EK::Vector_3> piece_draw_dirs;
        std::vector<std::string> piece_colors = {"#2bee2b80", "#2b80ee80", "#ee802b80", "#ee2b8080", "#80ee2b80", "#802bee80"};

        int piece_idx = 1;
        while (handled_faces_count < total_faces && piece_idx <= 10) {
            auto opt = mold::optimize_parting_direction(mesh_part, face_normals, edge_to_faces, is_handled, params);
            if (opt.source_faces.empty() || opt.solid_wedge.number_of_faces() == 0) {
                break;
            }

            EK::Vector_3 d_i = opt.best_dir;
            auto wedge = opt.solid_wedge;
            piece_draw_dirs.push_back(d_i);

            // Mark source faces as handled
            for (size_t f_idx : opt.source_faces) {
                auto f = mold::ExactMesh::Face_index(f_idx);
                if (!is_handled[f]) {
                    is_handled[f] = true;
                    handled_faces_count++;
                }
            }

            // Corefine conservative stock intersection & model cavity difference
            mold::ExactMesh raw_block;
            CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(conservative_stock, wedge, raw_block);
            mold::ExactMesh piece_mesh;
            CGAL::Polygon_mesh_processing::corefine_and_compute_difference(raw_block, mesh_part, piece_mesh);

            std::string color = piece_colors[(piece_idx - 1) % piece_colors.size()];
            std::string piece_name = "mold_piece_" + std::to_string(piece_idx);
            mold_pieces.push_back({piece_mesh, d_i, piece_name, color, piece_idx});

            std::cout << "  - Extracted " << piece_name << " along dir ("
                      << CGAL::to_double(d_i.x()) << ", " << CGAL::to_double(d_i.y()) << ", " << CGAL::to_double(d_i.z())
                      << ") covering " << opt.source_faces.size() << " faces (total handled: "
                      << handled_faces_count << " / " << total_faces << ")." << std::endl << std::flush;

            piece_idx++;
        }

        // 6. Stage 2: Minimal-Volume OBB Assembly Trimming
        Geometry obb_geo;
        if (!mold_pieces.empty()) {
            std::cout << "    [OBB] Computing Minimal-Volume OBB trim with padding " << CGAL::to_double(params.padding) << "..." << std::flush;
            auto opt_obb = mold::compute_min_volume_obb(mesh_part, params.padding, piece_draw_dirs);
            obb_geo = opt_obb.to_geometry();
            mold::ExactMesh obb_mesh = boolean::Engine::geometry_to_mesh(obb_geo);
            std::cout << " Done. Min Volume: " << CGAL::to_double(opt_obb.volume) << std::endl << std::flush;

            for (auto& piece : mold_pieces) {
                mold::ExactMesh trimmed_piece;
                CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(piece.mesh, obb_mesh, trimmed_piece);
                if (trimmed_piece.number_of_faces() > 0) {
                    piece.mesh = trimmed_piece;
                }
            }
        }

        // 7. Demoldability Verification
        mold::Tree model_tree(CGAL::faces(mesh_part).first, CGAL::faces(mesh_part).second, mesh_part);
        model_tree.build();
        for (const auto& piece : mold_pieces) {
            mold::verify_piece_demoldability(piece, model_tree);
        }

        // 8. Assemble Final Scene Graph & Apply Explosion Transforms
        Shape result;
        result.tf = Matrix::identity();

        for (const auto& piece : mold_pieces) {
            Geometry piece_geo = boolean::Engine::mesh_to_geometry(piece.mesh);
            Shape piece_shape = P::make_shape(vfs, piece_geo, {
                {"mold_piece", piece.mold_piece},
                {"color", piece.color}
            });

            if (params.explode > FT(0)) {
                EK::Vector_3 dv = piece.draw_vector;
                double len = std::sqrt(CGAL::to_double(dv.squared_length()));
                if (len > 1e-6) {
                    FT scale = params.explode / FT(len);
                    piece_shape.tf = Matrix::translate(CGAL::to_double(dv.x() * scale), CGAL::to_double(dv.y() * scale), CGAL::to_double(dv.z() * scale));
                }
            }
            result.components.push_back(piece_shape);
        }

        // Include Minimal-Volume OBB as ghost outline/reference
        if (!obb_geo.vertices.empty()) {
            Shape obb_shape = P::make_shape(vfs, obb_geo, {
                {"role", "ghost"},
                {"color", "#ffffff25"},
                {"name", "minimal_bounding_box"}
            });
            result.components.push_back(obb_shape);
        }

        // Keep original input model as-is in the result
        result.components.push_back(in);

        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "padding", "explode", "draft"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/mold"},
            {"dsl_name", "mold"},
            {"role", "method"},
            {"description", "Decomposes a watertight 3D solid geometry into interlocking certified 2-manifold solid mold blocks with 3D parting sheets and slide inserts."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"binding", "implicit"}, {"description", "The watertight solid shape to decompose into molds."}}}
            }},
            {"arguments", {
                {{"name", "padding"}, {"type", "jot:number"}, {"default", 10.0}, {"description", "Stock mold block wall thickness padding in mm."}},
                {{"name", "explode"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Explosion distance along piece withdrawal vectors in mm."}},
                {{"name", "draft"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Minimum draft angle in turns (tau, where 1.0 = 360 degrees)."}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The multi-piece mold assembly containing mold blocks and the centered model."}}}
            }}
        };
    }
};

inline void mold_init(fs::VFSNode* vfs) {
    Processor::register_op<MoldOp<>, Shape, double, double, double>(vfs, "jot/mold");
}

} // namespace geo
} // namespace jotcad
