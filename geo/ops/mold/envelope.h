#pragma once

#include "types.h"
#include "rotation.h"
#include "stock_footprint.h"
#include "wedge.h"
#include <CGAL/envelope_3.h>
#include <CGAL/Env_triangle_traits_3.h>
#include <CGAL/Env_surface_data_traits_3.h>
#include <queue>
#include <vector>
#include <set>
#include <map>
#include <chrono>
#include <mutex>

namespace jotcad {
namespace geo {
namespace mold {

// Type alias for backward compatibility
typedef EnvelopeWedgeResult EnvelopeMeshResult;

inline bool is_visible(const EK::Vector_3& normal, const EK::Vector_3& d, const FT& min_dot = FT(0)) {
    return normal * d >= min_dot;
}

template <typename FaceHandledMap = FaceBoolMap>
inline std::vector<ExactMesh::Face_index> compute_visible_patch_faces_fast(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    FaceHandledMap is_handled,
    const EK::Vector_3& d,
    FT min_dot
) {
    std::vector<ExactMesh::Face_index> visible_faces;
    for (auto f : face_descriptors) {
        if (!is_handled[f] && is_visible(face_normals[f.idx()], d, min_dot)) {
            visible_faces.push_back(f);
        }
    }
    return visible_faces;
}

inline bool build_solid_wedge_from_context(
    EnvelopeMeshResult& res,
    const FT& padding = FT(10),
    const ExactMesh* stock_mesh = nullptr
);

/**
 * @brief Computes the exact 3D upper envelope and constructs the certified solid demolding wedge.
 * 
 * Rotates the target pull direction to +Z, extracts the largest connected visible surface component,
 * computes CGAL::upper_envelope_3, and delegates solid wedge extrusion and regularization to wedge.h.
 */
inline EnvelopeMeshResult compute_exact_upper_envelope_mesh(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const EK::Vector_3& d,
    const FT& padding = FT(10),
    const ExactMesh* stock_mesh = nullptr,
    bool build_wedge = true
) {
    auto [to_z, from_z] = compute_exact_z_rotation(d);

    // The withdrawal shaft is swept strictly by forward-facing geometry along d (min_dot = 0)
    auto is_forward_facing = [&](ExactMesh::Face_index f) -> bool {
        return is_visible(face_normals[f.idx()], d);
    };

    std::set<ExactMesh::Face_index> candidate_faces;
    for (auto f : face_descriptors) {
        if (is_forward_facing(f)) {
            candidate_faces.insert(f);
        }
    }

    if (candidate_faces.empty()) return {};

    typedef CGAL::Env_triangle_traits_3<EK> Triangle_traits_3;
    typedef CGAL::Env_surface_data_traits_3<Triangle_traits_3, size_t> Traits_3;
    typedef Traits_3::Surface_3 Data_triangle_3;
    typedef CGAL::Envelope_diagram_2<Traits_3> Envelope_diagram_2;

    std::vector<Data_triangle_3> triangles;
    std::map<size_t, std::vector<EK::Point_3>> rotated_tris;

    // 1. Collect non-degenerate upper triangles from candidate_faces
    for (auto f : candidate_faces) {
        size_t f_idx = f.idx();
        auto h = mesh_part.halfedge(f);
        auto p0 = to_z(mesh_part.point(mesh_part.source(h)));
        auto p1 = to_z(mesh_part.point(mesh_part.target(h)));
        auto p2 = to_z(mesh_part.point(mesh_part.target(mesh_part.next(h))));

        // Flatten lazy expression trees to pure rational leaves (depth = 0)
        EK::Point_3 p0_flat(EK::FT(p0.x().exact()), EK::FT(p0.y().exact()), EK::FT(p0.z().exact()));
        EK::Point_3 p1_flat(EK::FT(p1.x().exact()), EK::FT(p1.y().exact()), EK::FT(p1.z().exact()));
        EK::Point_3 p2_flat(EK::FT(p2.x().exact()), EK::FT(p2.y().exact()), EK::FT(p2.z().exact()));

        // Strictly filter for non-degenerate, positive-orientation (upward functional surface) in 2D projection
        FT area_2d = (p1_flat.x() - p0_flat.x()) * (p2_flat.y() - p0_flat.y()) - (p1_flat.y() - p0_flat.y()) * (p2_flat.x() - p0_flat.x());
        if (area_2d > FT(0)) {
            EK::Triangle_3 tri(p0_flat, p1_flat, p2_flat);
            triangles.push_back(Data_triangle_3(tri, f_idx));
            rotated_tris[f_idx] = {p0_flat, p1_flat, p2_flat};
        }
    }

    if (triangles.empty()) return {};

    auto diag_ptr = std::make_shared<Envelope_diagram_2>();
    auto t_env_start = std::chrono::steady_clock::now();

    CGAL::upper_envelope_3(triangles.begin(), triangles.end(), *diag_ptr);

    auto t_env_end = std::chrono::steady_clock::now();
    double env_ms = std::chrono::duration<double, std::milli>(t_env_end - t_env_start).count();
    {
        static std::mutex s_log_mtx;
        std::lock_guard<std::mutex> lock(s_log_mtx);
        std::cout << "    [Envelope] Computing CGAL::upper_envelope_3 on " << triangles.size()
                  << " triangles... Done in " << env_ms << "ms (faces in diagram: "
                  << diag_ptr->number_of_faces() << ")." << std::endl << std::flush;
    }


    EnvelopeMeshResult res;
    res.has_solid_wedge = false;
    for (auto fit = diag_ptr->faces_begin(); fit != diag_ptr->faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        for (auto sit = fit->surfaces_begin(); sit != fit->surfaces_end(); ++sit) {
            res.source_faces.insert(sit->data());
        }
    }

    res.context = std::make_shared<EnvelopeContext>();
    res.context->diag = diag_ptr;
    res.context->rotated_tris = std::move(rotated_tris);
    res.context->to_z = to_z;
    res.context->from_z = from_z;

    if (build_wedge) {
        build_solid_wedge_from_context(res, padding, stock_mesh);
    }
    return res;
}

/**
 * @brief Constructs the 3D solid wedge lazily on demand from an already computed EnvelopeContext.
 */
inline bool build_solid_wedge_from_context(
    EnvelopeMeshResult& res,
    const FT& padding,
    const ExactMesh* stock_mesh
) {
    if (!res.context || !res.context->diag) return false;
    auto& max_diag = *res.context->diag;
    const auto& rotated_tris = res.context->rotated_tris;
    const auto& to_z = res.context->to_z;
    const auto& from_z = res.context->from_z;

    auto get_z = [&](size_t orig_f_idx, const FT& vx, const FT& vy) -> FT {
        auto it = rotated_tris.find(orig_f_idx);
        if (it == rotated_tris.end()) return FT(0);
        const auto& tri_pts = it->second;
        const auto& p0 = tri_pts[0];
        const auto& p1 = tri_pts[1];
        const auto& p2 = tri_pts[2];
        EK::Vector_3 n = CGAL::cross_product(p1 - p0, p2 - p0);
        if (n.z() == FT(0)) return p0.z();
        return p0.z() - (n.x() * (vx - p0.x()) + n.y() * (vy - p0.y())) / n.z();
    };

    FT max_vz_rot = -1000000;
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f_idx = fit->surfaces_begin()->data();
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            auto p2d = curr->target()->point();
            FT vz = get_z(orig_f_idx, p2d.x(), p2d.y());
            if (vz > max_vz_rot) max_vz_rot = vz;
            curr = curr->next();
        } while (curr != ccb);
    }

    FT h_ceiling_rot = max_vz_rot + (padding > FT(0) ? padding : FT(1));
    if (stock_mesh != nullptr && !stock_mesh->is_empty()) {
        StockFootprint fp = compute_stock_footprint(*stock_mesh, to_z);
        h_ceiling_rot = fp.w_max + (padding > FT(0) ? padding : FT(1));
    }

    // Delegate solid wedge extrusion, solid-aware soup repair, and world-space transformation
    EnvelopeWedgeResult wedge_res = construct_envelope_wedge(max_diag, get_z, h_ceiling_rot, from_z, to_z);
    res.solid_wedge = std::move(wedge_res.solid_wedge);
    res.total_area = wedge_res.total_area;
    res.boundary_loops_3d = std::move(wedge_res.boundary_loops_3d);
    for (size_t f : wedge_res.source_faces) {
        res.source_faces.insert(f);
    }
    res.has_solid_wedge = !res.solid_wedge.is_empty();
    return res.has_solid_wedge;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
