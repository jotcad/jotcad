#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "mold/types.h"
#include "mold/repair.h"
#include "mold/optimizer.h"
#include "mold/assembly.h"
#include "mold/verify.h"
#include "fix/assert_mesh.h"
#include "boolean/corefine.h"
#include <iostream>
#include <stdexcept>

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
        double draft_val = 0.0,
        std::string kiss_val = "weld",
        double kiss_width_val = 0.01
    ) {
        mold::MoldParams params;
        params.padding = FT(padding_val);
        params.explode = FT(explode_val);
        params.draft = FT(draft_val);

        if (kiss_val == "weld") {
            params.kiss_mode = fix::KissMode::WELD;
        } else if (kiss_val == "part") {
            params.kiss_mode = fix::KissMode::PART;
        } else {
            throw std::runtime_error("Invalid kiss mode '" + kiss_val + "': must be 'weld' or 'part'");
        }
        params.kiss_width = FT(kiss_width_val);

        // 1. Extract unified solid mesh in world space
        mold::ExactMesh mesh_part;
        if (!boolean::Engine::shape_to_fused_mesh(vfs, in, mesh_part)) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        fix::assert_well_formed_mesh(mesh_part, "mesh_part in MoldOp");

        // 3. Topology & Geometric Centroids / Normals in Pure FT
        std::map<mold::EdgeKey, std::vector<int>> edge_to_faces;
        std::vector<EK::Vector_3> face_normals;
        std::vector<EK::Point_3> face_centroids;

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

        // 4. Large Conservative Stock Envelope for Unconstrained Extraction
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
        fix::assert_well_formed_mesh(conservative_stock, "conservative_stock in MoldOp");

        // 5. Multi-Piece Mold Decomposition Loop
        mold::FaceBoolMap is_handled = mesh_part.add_property_map<mold::ExactMesh::Face_index, bool>("f:is_handled", false).first;
        size_t total_faces = mesh_part.number_of_faces();
        size_t handled_faces_count = 0;

        std::vector<mold::MoldPiece> mold_pieces;
        std::vector<EK::Vector_3> piece_draw_dirs;
        const std::vector<std::string> piece_colors = {"#2bee2b", "#2b80ee", "#ee802b", "#ee2b80", "#80ee2b", "#802bee"};

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
            for (size_t src_f_idx : opt.source_faces) {
                auto f = mold::ExactMesh::Face_index(src_f_idx);
                if (!is_handled[f]) {
                    is_handled[f] = true;
                    handled_faces_count++;
                }
            }

            // Corefine conservative stock intersection & model cavity difference
            mold::ExactMesh stock_copy = conservative_stock;
            mold::ExactMesh raw_block;
            bool ok_inter = boolean::corefine_intersection(stock_copy, wedge, raw_block, params.kiss_mode, params.kiss_width, "stock ∩ wedge in MoldOp");
            assert(ok_inter && "stock_copy ∩ wedge failed in MoldOp!");

            mold::ExactMesh model_copy = mesh_part;
            mold::ExactMesh piece_mesh;
            bool ok_diff = boolean::corefine_difference(raw_block, model_copy, piece_mesh, params.kiss_mode, params.kiss_width, "raw_block \\ model_copy in MoldOp");
            assert(ok_diff && "raw_block \\ model_copy failed in MoldOp!");
            fix::assert_well_formed_mesh(piece_mesh, "piece_mesh in MoldOp");

            std::string color = piece_colors[(piece_idx - 1) % piece_colors.size()];
            std::string piece_name = "mold_piece_" + std::to_string(piece_idx);
            mold_pieces.push_back({piece_mesh, d_i, piece_name, color, piece_idx});

            std::cout << "  - Extracted " << piece_name << " along dir ("
                      << CGAL::to_double(d_i.x()) << ", " << CGAL::to_double(d_i.y()) << ", " << CGAL::to_double(d_i.z())
                      << ") covering " << opt.source_faces.size() << " faces (total handled: "
                      << handled_faces_count << " / " << total_faces << ")." << std::endl << std::flush;

            piece_idx++;
        }

        // 6. Minimal-Volume OBB Trimming & Stationary Remainder Extraction
        Geometry obb_geo;
        mold::MoldAssembly<P>::trim_against_obb(mesh_part, params, mold_pieces, piece_draw_dirs, obb_geo);

        // 7. Demoldability Verification
        mold::Tree model_tree(CGAL::faces(mesh_part).first, CGAL::faces(mesh_part).second, mesh_part);
        model_tree.build();
        for (const auto& piece : mold_pieces) {
            mold::verify_piece_demoldability(piece, model_tree);
        }

        // 8. Assemble Scene Graph & Apply Explosion Transforms
        Shape result = mold::MoldAssembly<P>::assemble_scene(vfs, in, mold_pieces, obb_geo, params);
        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "padding", "explode", "draft", "kiss", "kiss_width"}; }
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
                {{"name", "draft"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Minimum draft angle in turns (tau, where 1.0 = 360 degrees)."}},
                {{"name", "kiss"}, {"type", "jot:string"}, {"default", "weld"}, {"description", "Resolution mode for zero-volume contact singularities ('weld' or 'part')."}},
                {{"name", "kiss_width"}, {"type", "jot:number"}, {"default", 0.01}, {"description", "Physical width in mm of structural bridge ('weld') or clearance gap ('part')."}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The multi-piece mold assembly containing mold blocks and the centered model."}}}
            }}
        };
    }
};

inline void mold_init(fs::VFSNode* vfs) {
    Processor::register_op<MoldOp<>, Shape, double, double, double, std::string, double>(vfs, "jot/mold");
}

} // namespace geo
} // namespace jotcad
