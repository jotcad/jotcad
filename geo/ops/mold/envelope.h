#pragma once

#include "types.h"
#include "rotation.h"
#include "wedge.h"
#include <CGAL/envelope_3.h>
#include <CGAL/Env_triangle_traits_3.h>
#include <CGAL/Env_surface_data_traits_3.h>
#include <queue>
#include <vector>
#include <set>
#include <map>
#include <chrono>

namespace jotcad {
namespace geo {
namespace mold {

// Type alias for backward compatibility
typedef EnvelopeWedgeResult EnvelopeMeshResult;

inline bool is_visible(const EK::Vector_3& normal, const EK::Vector_3& d, const FT& min_dot = FT(0)) {
    return normal * d >= min_dot;
}

inline std::vector<ExactMesh::Face_index> compute_visible_patch_faces_fast(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    FaceBoolMap is_handled,
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
    FaceBoolMap is_handled,
    const EK::Vector_3& d,
    const std::vector<ExactMesh::Face_index>& seed_patch_faces = {}
) {
    auto [to_z, from_z] = compute_exact_z_rotation(d);

    // The withdrawal shaft is swept strictly by forward-facing geometry along d (min_dot = 0)
    auto is_forward_facing = [&](ExactMesh::Face_index f) -> bool {
        return is_visible(face_normals[f.idx()], d);
    };

    std::set<ExactMesh::Face_index> candidate_faces;
    bool has_seed = !seed_patch_faces.empty();
    if (has_seed) {
        std::queue<ExactMesh::Face_index> q;
        for (auto f : seed_patch_faces) {
            if (is_forward_facing(f)) {
                candidate_faces.insert(f);
                q.push(f);
            }
        }
        while (!q.empty()) {
            auto curr_f = q.front();
            q.pop();
            auto h = mesh_part.halfedge(curr_f);
            auto h_start = h;
            do {
                auto h_twin = mesh_part.opposite(h);
                if (h_twin != ExactMesh::null_halfedge()) {
                    auto neighbor_f = mesh_part.face(h_twin);
                    if (neighbor_f != ExactMesh::null_face() && !is_handled[neighbor_f]) {
                        if (is_forward_facing(neighbor_f)) {
                            if (candidate_faces.insert(neighbor_f).second) {
                                q.push(neighbor_f);
                            }
                        }
                    }
                }
                h = mesh_part.next(h);
            } while (h != h_start);
        }
    } else {
        std::set<ExactMesh::Face_index> eligible;
        for (auto f : face_descriptors) {
            if (!is_handled[f] && is_forward_facing(f)) {
                eligible.insert(f);
            }
        }
        std::set<ExactMesh::Face_index> visited;
        std::vector<std::vector<ExactMesh::Face_index>> components;
        for (auto f : eligible) {
            if (visited.count(f)) continue;
            std::vector<ExactMesh::Face_index> comp;
            std::queue<ExactMesh::Face_index> q;
            q.push(f);
            visited.insert(f);
            while (!q.empty()) {
                auto curr = q.front();
                q.pop();
                comp.push_back(curr);
                auto h = mesh_part.halfedge(curr);
                auto h_start = h;
                do {
                    auto h_twin = mesh_part.opposite(h);
                    if (h_twin != ExactMesh::null_halfedge()) {
                        auto neighbor_f = mesh_part.face(h_twin);
                        if (neighbor_f != ExactMesh::null_face() && eligible.count(neighbor_f) && !visited.count(neighbor_f)) {
                            visited.insert(neighbor_f);
                            q.push(neighbor_f);
                        }
                    }
                    h = mesh_part.next(h);
                } while (h != h_start);
            }
            components.push_back(comp);
        }

        if (components.empty()) return {};

        size_t best_c = 0;
        FT best_area = FT(0);
        for (size_t c = 0; c < components.size(); ++c) {
            FT a = FT(0);
            for (auto f : components[c]) {
                auto h = mesh_part.halfedge(f);
                auto p0 = mesh_part.point(mesh_part.source(h));
                auto p1 = mesh_part.point(mesh_part.target(h));
                auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
                a += std::sqrt(CGAL::to_double(CGAL::cross_product(p1 - p0, p2 - p0).squared_length())) / 2.0;
            }
            if (a > best_area) {
                best_area = a;
                best_c = c;
            }
        }
        candidate_faces.insert(components[best_c].begin(), components[best_c].end());
    }

    if (candidate_faces.empty()) return {};

    typedef CGAL::Env_triangle_traits_3<EK> Triangle_traits_3;
    typedef CGAL::Env_surface_data_traits_3<Triangle_traits_3, size_t> Traits_3;
    typedef Traits_3::Surface_3 Data_triangle_3;

    std::vector<Data_triangle_3> triangles;
    std::map<size_t, std::vector<EK::Point_3>> rotated_tris;

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

    std::cout << "    [Envelope] OBB Corridor filtered: " << triangles.size() << " / " << face_descriptors.size() << " candidate triangles." << std::endl << std::flush;
    std::cout << "    [Envelope] Computing CGAL::upper_envelope_3 on " << triangles.size() << " triangles..." << std::flush;
    auto t_env_start = std::chrono::steady_clock::now();

    Envelope_diagram_2 max_diag;
    CGAL::upper_envelope_3(triangles.begin(), triangles.end(), max_diag);

    auto t_env_end = std::chrono::steady_clock::now();
    double env_ms = std::chrono::duration<double, std::milli>(t_env_end - t_env_start).count();
    std::cout << " Done in " << env_ms << "ms (faces in diagram: " << max_diag.number_of_faces() << ")." << std::endl << std::flush;

    auto get_z = [&](size_t orig_f_idx, const FT& vx, const FT& vy) -> FT {
        const auto& tri_pts = rotated_tris[orig_f_idx];
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

    FT h_ceiling_rot = max_vz_rot + FT(50);

    // Delegate solid wedge extrusion, solid-aware soup repair, and world-space transformation
    return construct_envelope_wedge(max_diag, get_z, h_ceiling_rot, from_z, to_z);
}

} // namespace mold
} // namespace geo
} // namespace jotcad
