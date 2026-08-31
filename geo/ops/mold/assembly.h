#pragma once
#include "types.h"
#include "obb.h"
#include "verify.h"
#include <sstream>

namespace jotcad {
namespace geo {
namespace mold {

template <typename P = JotVfsProtocol>
struct MoldAssembly {
    static void trim_against_obb(
        const ExactMesh& mesh_part,
        const MoldParams& params,
        std::vector<MoldPiece>& mold_pieces,
        const std::vector<EK::Vector_3>& piece_draw_dirs,
        Geometry& out_obb_geo
    ) {
        if (mold_pieces.empty()) return;

        std::cout << "    [OBB] Computing Minimal-Volume OBB trim with padding " << CGAL::to_double(params.padding) << "..." << std::flush;
        auto opt_obb = compute_min_volume_obb(mesh_part, params.padding, piece_draw_dirs);
        out_obb_geo = opt_obb.to_geometry();
        ExactMesh obb_mesh = boolean::Engine::geometry_to_mesh(out_obb_geo);
        std::cout << " Done. Min Volume: " << CGAL::to_double(opt_obb.volume) << std::endl << std::flush;

        for (auto& piece : mold_pieces) {
            ExactMesh piece_in = piece.mesh;
            ExactMesh obb_in = obb_mesh;
            ExactMesh trimmed_piece;
            CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(piece_in, obb_in, trimmed_piece);
            if (trimmed_piece.number_of_faces() > 0) {
                piece.mesh = trimmed_piece;
            }
        }

        // Extract stationary foundation remainder block from uncarved OBB stock
        ExactMesh obb_copy = obb_mesh;
        ExactMesh model_copy = mesh_part;
        ExactMesh final_remaining;
        CGAL::Polygon_mesh_processing::corefine_and_compute_difference(obb_copy, model_copy, final_remaining);
        for (const auto& piece : mold_pieces) {
            ExactMesh piece_copy = piece.mesh;
            ExactMesh next_rem;
            CGAL::Polygon_mesh_processing::corefine_and_compute_difference(final_remaining, piece_copy, next_rem);
            if (next_rem.number_of_faces() > 0) {
                final_remaining = next_rem;
            }
        }

        if (final_remaining.number_of_faces() > 0 && CGAL::is_closed(final_remaining) && CGAL::Polygon_mesh_processing::volume(final_remaining) > FT(1)) {
            const std::vector<std::string> piece_colors = {"#2bee2b", "#2b80ee", "#ee802b", "#ee2b80", "#80ee2b", "#802bee"};
            int next_piece_idx = (int)mold_pieces.size() + 1;
            std::string color = piece_colors[(next_piece_idx - 1) % piece_colors.size()];
            std::string piece_name = "mold_piece_" + std::to_string(next_piece_idx);
            EK::Vector_3 base_dir(FT(0), FT(0), FT(0)); // Stationary foundation: pull vector "0 0 0"
            mold_pieces.push_back({final_remaining, base_dir, piece_name, color, next_piece_idx});
        }
    }

    static Shape assemble_scene(
        fs::VFSNode* vfs,
        const Shape& original_input,
        const std::vector<MoldPiece>& mold_pieces,
        const Geometry& obb_geo,
        const MoldParams& params
    ) {
        Shape result = original_input;
        result.geometry = std::nullopt; // Consumed raw solid geometry
        result.components.clear();

        for (const auto& piece : mold_pieces) {
            Geometry piece_geo = boolean::Engine::mesh_to_geometry(piece.mesh);
            std::stringstream ss;
            ss << piece.draw_vector.x() << " " << piece.draw_vector.y() << " " << piece.draw_vector.z();
            std::string pull_vec_str = ss.str();

            Shape piece_shape = P::make_shape(vfs, piece_geo, {
                {"mold/piece", piece.mold_piece},
                {"mold/pull_vector", pull_vec_str},
                {"color", piece.color},
                {"opacity", 0.5}
            });

            if (params.explode > FT(0)) {
                EK::Vector_3 dv = piece.draw_vector;
                double len = std::sqrt(CGAL::to_double(dv.squared_length()));
                if (len > 1e-9) {
                    FT scale = params.explode / FT(len);
                    EK::Vector_3 trans = dv * scale;
                    piece_shape.tf = Matrix(Transformation(CGAL::TRANSLATION, trans));
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

        // Naturally preserve all unconsumed child branches of the input tree
        for (const auto& child : original_input.components) {
            result.components.push_back(child);
        }

        return result;
    }
};

} // namespace mold
} // namespace geo
} // namespace jotcad
