#pragma once
#include "types.h"
#include "obb.h"
#include "verify.h"
#include "fix/assert_mesh.h"
#include "boolean/corefine.h"
#include <sstream>

namespace jotcad {
namespace geo {
namespace mold {

template <typename P = JotVfsProtocol>
struct MoldAssembly {
    static void trim_against_obb(
        fs::VFSNode* vfs,
        const Shape& original_input,
        const ExactMesh& mesh_part,
        const MoldParams& params,
        std::vector<MoldPiece>& mold_pieces,
        const std::vector<EK::Vector_3>& piece_draw_dirs,
        Geometry& out_obb_geo
    ) {
        if (mold_pieces.empty()) return;

        ExactMesh obb_mesh;
        bool found_box = false;
        for (const auto& child : original_input.components) {
            if (child.has_tag("mold/role", "box") && child.geometry.has_value()) {
                std::cout << "    [OBB] Using attached mold stock box component..." << std::flush;
                out_obb_geo = vfs->template readCID<Geometry>(*child.geometry);
                obb_mesh = boolean::Engine::geometry_to_mesh(out_obb_geo);
                boolean::Engine::transform_mesh(obb_mesh, child.tf);
                found_box = true;
                std::cout << " Done." << std::endl << std::flush;
                break;
            }
        }

        if (!found_box) {
            std::cout << "    [OBB] Computing Minimal-Volume OBB trim with padding " << CGAL::to_double(params.padding) << "..." << std::flush;
            auto opt_obb = compute_min_volume_obb(mesh_part, params.padding, piece_draw_dirs);
            out_obb_geo = opt_obb.to_geometry();
            obb_mesh = boolean::Engine::geometry_to_mesh(out_obb_geo);
            std::cout << " Done. Min Volume: " << CGAL::to_double(opt_obb.volume) << std::endl << std::flush;
        }
        fix::assert_well_formed_for_corefinement(obb_mesh, "obb_mesh in MoldAssembly::trim_against_obb");

        for (auto& piece : mold_pieces) {
            ExactMesh trimmed_piece;
            boolean::corefine_intersection(piece.mesh, obb_mesh, trimmed_piece, params.kiss_mode, params.kiss_width, piece.name + " OBB trim");
            piece.mesh = trimmed_piece;
            if (trimmed_piece.is_empty() || trimmed_piece.number_of_faces() == 0) {
                std::cout << "    [OBB Trim] " << piece.name << " was outside stock envelope; preserved as empty piece for provenance." << std::endl;
            }
        }

        // Extract stationary foundation remainder block from uncarved OBB stock
        ExactMesh final_remaining;
        boolean::corefine_difference(obb_mesh, mesh_part, final_remaining, params.kiss_mode, params.kiss_width, "obb \\ model in assembly");

        for (const auto& piece : mold_pieces) {
            if (piece.mesh.is_empty() || piece.mesh.number_of_faces() == 0) continue;
            ExactMesh next_rem;
            boolean::corefine_difference(final_remaining, piece.mesh, next_rem, params.kiss_mode, params.kiss_width, "final_remaining \\ " + piece.name);
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

        // 1. Preserve the center model solid geometry tagged with mold/role="model"
        if (original_input.geometry.has_value()) {
            Shape center_model = original_input;
            center_model.components.clear(); // Children (sprue/vents) are preserved below
            center_model.tags["mold/role"] = "model";
            result.components.push_back(center_model);
        }

        for (const auto& piece : mold_pieces) {
            Geometry piece_geo = boolean::Engine::mesh_to_geometry(piece.mesh);
            std::stringstream ss;
            ss << piece.draw_vector.x() << " " << piece.draw_vector.y() << " " << piece.draw_vector.z();
            std::string pull_vec_str = ss.str();

            Shape piece_shape = P::make_shape(vfs, piece_geo, {
                {"mold/role", "piece"},
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
                {"mold/role", "box"},
                {"role", "ghost"},
                {"color", "#ffffff25"},
                {"name", "minimal_bounding_box"}
            });
            result.components.push_back(obb_shape);
        }

        // Naturally preserve all unconsumed child branches of the input tree (skipping consumed mold stock box and construction ghosts)
        for (const auto& child : original_input.components) {
            if (child.has_tag("mold/role", "box") || child.is_ghost()) {
                continue; // Consumed as mold pieces or CAD construction ghost!
            }
            result.components.push_back(child);
        }

        return result;
    }
};

} // namespace mold
} // namespace geo
} // namespace jotcad
