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
        double explode_val = 0.0
    ) {
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

        // 4. Bounding Box & Padding Margin
        FT pad(padding_val);
        FT mx_min = b_xmin - pad, mx_max = b_xmax + pad;
        FT my_min = b_ymin - pad, my_max = b_ymax + pad;
        FT mz_min = b_zmin - pad, mz_max = b_zmax + pad;
        EK::Point_3 center((b_xmin + b_xmax) / 2, (b_ymin + b_ymax) / 2, (b_zmin + b_zmax) / 2);

        // 5. Recursive Progressive Decomposition Loop
        Geometry stock_geo = mold::build_box_geo(mx_min, mx_max, my_min, my_max, mz_min, mz_max);
        mold::ExactMesh remaining_stock = boolean::Engine::geometry_to_mesh(stock_geo);

        mold::FaceBoolMap is_handled = mesh_part.add_property_map<mold::ExactMesh::Face_index, bool>("f:handled", false).first;
        int remaining_face_count = (int)mesh_part.number_of_faces();

        std::vector<mold::MoldPiece> mold_pieces;
        const std::vector<std::string> piece_colors = {"#ee2b2b80", "#2bee2b80", "#2b2bee80", "#ee882b80", "#882bee80"};
        int piece_idx = 1;

        while (remaining_face_count > 0) {
            auto opt = mold::optimize_parting_direction(mesh_part, face_normals, edge_to_faces, is_handled);

            auto wedge_k = mold::build_parting_solid_wedge(
                mesh_part, opt.positive_patch_faces, opt.boundary_cycle,
                opt.best_dir, center, mx_min, mx_max, my_min, my_max, mz_min, mz_max
            );

            mold::ExactMesh raw_block;
            CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(remaining_stock, wedge_k, raw_block);

            mold::ExactMesh carved_piece;
            CGAL::Polygon_mesh_processing::corefine_and_compute_difference(raw_block, mesh_part, carved_piece);

            std::string piece_name = "mold_piece_" + std::to_string(piece_idx);
            std::string color = piece_colors[(piece_idx - 1) % piece_colors.size()];

            mold_pieces.push_back({carved_piece, opt.best_dir, piece_name, color, piece_idx});

            mold::ExactMesh next_stock;
            CGAL::Polygon_mesh_processing::corefine_and_compute_difference(remaining_stock, raw_block, next_stock);
            remaining_stock = next_stock;

            for (auto f : opt.positive_patch_faces) {
                if (!is_handled[f]) {
                    is_handled[f] = true;
                    remaining_face_count--;
                }
            }
            piece_idx++;
        }

        // 6. Demoldability Verification
        mold::Tree model_tree(CGAL::faces(mesh_part).first, CGAL::faces(mesh_part).second, mesh_part);
        model_tree.build();
        for (const auto& piece : mold_pieces) {
            mold::verify_piece_demoldability(piece, model_tree);
        }

        // 7. Assemble Final Scene Graph & Apply Explosion Transforms
        Shape result;
        result.tf = Matrix::identity();

        FT explode(explode_val);
        for (const auto& piece : mold_pieces) {
            Geometry piece_geo = boolean::Engine::mesh_to_geometry(piece.mesh);
            Shape piece_shape = P::make_shape(vfs, piece_geo, {
                {"mold_piece", piece.mold_piece},
                {"color", piece.color}
            });

            if (explode > FT(0)) {
                EK::Vector_3 dv = piece.draw_vector;
                double len = std::sqrt(CGAL::to_double(dv.squared_length()));
                if (len > 1e-6) {
                    FT scale = explode / FT(len);
                    piece_shape.tf = Matrix::translate(CGAL::to_double(dv.x() * scale), CGAL::to_double(dv.y() * scale), CGAL::to_double(dv.z() * scale));
                }
            }
            result.components.push_back(piece_shape);
        }

        // Keep original input model as-is in the result
        result.components.push_back(in);

        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "padding", "explode"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/mold"},
            {"dsl_name", "mold"},
            {"role", "method"},
            {"description", "Decomposes a watertight 3D solid geometry into interlocking certified 2-manifold solid mold blocks with 3D parting sheets and slide inserts."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"binding", "implicit"}, {"description", "The watertight solid shape to decompose into molds."}}},
                {"padding", {{"type", "number"}, {"default", 10.0}, {"description", "Stock mold block wall thickness padding in mm."}}},
                {"explode", {{"type", "number"}, {"default", 0.0}, {"description", "Explosion distance along piece withdrawal vectors in mm."}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The multi-piece mold assembly containing mold blocks and the centered model."}}}
            }}
        };
    }
};

inline void mold_init(fs::VFSNode* vfs) {
    Processor::register_op<MoldOp<>, Shape, double, double>(vfs, "jot/mold");
}

} // namespace geo
} // namespace jotcad
