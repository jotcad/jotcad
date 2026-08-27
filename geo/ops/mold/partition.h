#pragma once
#include "types.h"
#include "prism.h"
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

namespace jotcad {
namespace geo {
namespace mold {

inline std::vector<MoldPiece> partition_mold_blocks(
    ExactMesh& mesh_part,
    const std::vector<UndercutCluster>& clusters,
    ExactMesh& mesh_v_sigma,
    const EK::Vector_3& d1,
    const EK::Vector_3& d2,
    FT mx_min, FT mx_max,
    FT my_min, FT my_max,
    FT mz_min, FT mz_max,
    FT pad
) {
    // 1. Stock Box & Solid Parting Wedge in Pure FT
    Geometry stock_geo = build_box_geo(mx_min, mx_max, my_min, my_max, mz_min, mz_max);
    ExactMesh stock_mesh = boolean::Engine::geometry_to_mesh(stock_geo);

    // 2. Single-Pass Unified Corefinement Boolean Extraction on Closed Solids
    ExactMesh mesh_b1, mesh_b2;
    std::array<std::optional<ExactMesh*>, 4> ops = {
        std::nullopt,                 // UNION
        std::make_optional(&mesh_b1), // INTERSECTION  (Stock ∩ V_sigma)
        std::make_optional(&mesh_b2), // TM1_MINUS_TM2 (Stock \ V_sigma)
        std::nullopt                  // TM2_MINUS_TM1
    };
    CGAL::Polygon_mesh_processing::corefine_and_compute_boolean_operations(
        stock_mesh, mesh_v_sigma, ops
    );

    // 3. Extract Undercut Clusters as Oriented Prismatic Slide Columns
    std::vector<MoldPiece> mold_pieces;
    const std::vector<std::string> insert_colors = {"#2b2bee80", "#ee882b80", "#882bee80", "#2beeee80", "#eeee2b80"};

    std::vector<ExactMesh::Face_index> face_descriptors;
    for (auto f : CGAL::faces(mesh_part)) {
        face_descriptors.push_back(f);
    }

    for (size_t c_idx = 0; c_idx < clusters.size(); ++c_idx) {
        const auto& c = clusters[c_idx];
        
        std::vector<EK::Point_3> patch_pts;
        for (int f_idx : c.face_indices) {
            auto f = face_descriptors[f_idx];
            auto h = mesh_part.halfedge(f);
            patch_pts.push_back(mesh_part.point(mesh_part.source(h)));
            patch_pts.push_back(mesh_part.point(mesh_part.target(h)));
            patch_pts.push_back(mesh_part.point(mesh_part.target(mesh_part.next(h))));
        }

        Geometry insert_box_geo = build_oriented_prism_geo(patch_pts, c.avg_normal, FT(1), pad + FT(50));
        ExactMesh mesh_insert = boolean::Engine::geometry_to_mesh(insert_box_geo);

        // Cut model out of insert
        boolean::Engine::cut_mesh_by_mesh(mesh_insert, mesh_part);

        // Cut insert space out of both primary solid halves
        ExactMesh mesh_insert_solid = boolean::Engine::geometry_to_mesh(insert_box_geo);
        boolean::Engine::cut_mesh_by_mesh(mesh_b1, mesh_insert_solid);
        boolean::Engine::cut_mesh_by_mesh(mesh_b2, mesh_insert_solid);

        std::string c_color = insert_colors[c_idx % insert_colors.size()];
        mold_pieces.push_back({
            mesh_insert,
            c.avg_normal,
            "mold_block_insert_" + std::to_string(c_idx + 1),
            c_color,
            (int)(c_idx + 3)
        });
    }

    // 4. Cut Model Cavity out of Primary Solid Halves
    boolean::Engine::cut_mesh_by_mesh(mesh_b1, mesh_part);
    boolean::Engine::cut_mesh_by_mesh(mesh_b2, mesh_part);

    // Insert primary halves at front of piece list
    mold_pieces.insert(mold_pieces.begin(), {
        mesh_b2, d2, "mold_piece_2", "#ee2b2b80", 2
    });
    mold_pieces.insert(mold_pieces.begin(), {
        mesh_b1, d1, "mold_piece_1", "#2bee2b80", 1
    });

    return mold_pieces;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
