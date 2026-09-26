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
    const std::vector<ExactMesh::Face_index>& seed_patch_faces = {},
    const FT& padding = FT(10),
    const TideParams& override_tide = {}
) {
    auto [to_z, from_z] = compute_exact_z_rotation(d);

    // The withdrawal shaft is swept strictly by forward-facing geometry along d (min_dot = 0)
    auto is_forward_facing = [&](ExactMesh::Face_index f) -> bool {
        return is_visible(face_normals[f.idx()], d);
    };

    std::set<ExactMesh::Face_index> candidate_faces;
    bool has_seed = !seed_patch_faces.empty();
    if (has_seed) {
        for (auto f : seed_patch_faces) {
            if (is_forward_facing(f)) {
                candidate_faces.insert(f);
            }
        }

        bool first_seed_v = true;
        FT seed_u_min = 0, seed_u_max = 0;
        FT seed_v_min = 0, seed_v_max = 0;
        FT seed_z_min = 0;

        for (auto f : candidate_faces) {
            auto h = mesh_part.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                auto p_rot = to_z(mesh_part.point(mesh_part.target(h)));
                FT u = p_rot.x().exact();
                FT v = p_rot.y().exact();
                FT z = p_rot.z().exact();
                if (first_seed_v) {
                    seed_u_min = seed_u_max = u;
                    seed_v_min = seed_v_max = v;
                    seed_z_min = z;
                    first_seed_v = false;
                } else {
                    if (u < seed_u_min) seed_u_min = u;
                    if (u > seed_u_max) seed_u_max = u;
                    if (v < seed_v_min) seed_v_min = v;
                    if (v > seed_v_max) seed_v_max = v;
                    if (z < seed_z_min) seed_z_min = z;
                }
                h = mesh_part.next(h);
            }
        }

        // Ingress all forward-facing model faces whose 2D projected bounding box overlaps
        // the candidate patch corridor and whose max_z >= seed_z_min (analytical occluders / overhangs)
        for (auto f : face_descriptors) {
            if (!is_forward_facing(f)) continue;
            if (candidate_faces.count(f)) continue;

            auto h = mesh_part.halfedge(f);
            FT f_u_min = 0, f_u_max = 0;
            FT f_v_min = 0, f_v_max = 0;
            FT f_z_max = 0;
            bool first_v = true;

            for (int i = 0; i < 3; ++i) {
                auto p_rot = to_z(mesh_part.point(mesh_part.target(h)));
                FT u = p_rot.x().exact();
                FT v = p_rot.y().exact();
                FT z = p_rot.z().exact();
                if (first_v) {
                    f_u_min = f_u_max = u;
                    f_v_min = f_v_max = v;
                    f_z_max = z;
                    first_v = false;
                } else {
                    if (u < f_u_min) f_u_min = u;
                    if (u > f_u_max) f_u_max = u;
                    if (v < f_v_min) f_v_min = v;
                    if (v > f_v_max) f_v_max = v;
                    if (z > f_z_max) f_z_max = z;
                }
                h = mesh_part.next(h);
            }

            // Check 2D corridor overlap
            if (f_u_max < seed_u_min || f_u_min > seed_u_max) continue;
            if (f_v_max < seed_v_min || f_v_min > seed_v_max) continue;

            // Check if it can occlude the seed patch (max z >= seed_z_min)
            if (f_z_max >= seed_z_min) {
                candidate_faces.insert(f);
            }
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
                a += CGAL::approximate_sqrt(CGAL::cross_product(p1 - p0, p2 - p0).squared_length()) / FT(2);
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

    struct VerticalFaceSegment {
        CDT_Kernel::Point_2 p_a;
        CDT_Kernel::Point_2 p_b;
        FT min_z;
    };
    std::vector<VerticalFaceSegment> vertical_segments;

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

    // 2. Collect all vertical model faces across the full part mesh parallel to draw direction d
    for (auto f : face_descriptors) {
        if (face_normals[f.idx()] * d == FT(0)) {
            auto h = mesh_part.halfedge(f);
            auto p0 = to_z(mesh_part.point(mesh_part.source(h)));
            auto p1 = to_z(mesh_part.point(mesh_part.target(h)));
            auto p2 = to_z(mesh_part.point(mesh_part.target(mesh_part.next(h))));

            CDT_Kernel::Point_2 pt0(EK::FT(p0.x().exact()), EK::FT(p0.y().exact()));
            CDT_Kernel::Point_2 pt1(EK::FT(p1.x().exact()), EK::FT(p1.y().exact()));
            CDT_Kernel::Point_2 pt2(EK::FT(p2.x().exact()), EK::FT(p2.y().exact()));

            std::vector<CDT_Kernel::Point_2> pts = {pt0, pt1, pt2};
            size_t best_i = 0, best_j = 1;
            FT max_d2 = (pts[1] - pts[0]).squared_length();
            FT d2_02 = (pts[2] - pts[0]).squared_length();
            if (d2_02 > max_d2) { max_d2 = d2_02; best_i = 0; best_j = 2; }
            FT d2_12 = (pts[2] - pts[1]).squared_length();
            if (d2_12 > max_d2) { max_d2 = d2_12; best_i = 1; best_j = 2; }

            if (max_d2 > FT(0)) {
                FT z0 = EK::FT(p0.z().exact());
                FT z1 = EK::FT(p1.z().exact());
                FT z2 = EK::FT(p2.z().exact());
                FT min_z = (z0 < z1) ? ((z0 < z2) ? z0 : z2) : ((z1 < z2) ? z1 : z2);
                vertical_segments.push_back({pts[best_i], pts[best_j], min_z});
            }
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
    FT min_border_z = 1000000;
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f_idx = fit->surfaces_begin()->data();
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            auto p2d = curr->target()->point();
            FT vz = get_z(orig_f_idx, p2d.x(), p2d.y());
            if (vz > max_vz_rot) max_vz_rot = vz;

            auto twin_face = curr->twin()->face();
            if (twin_face->is_unbounded() || twin_face->number_of_surfaces() == 0) {
                auto p_src = curr->source()->point();
                FT z_s = get_z(orig_f_idx, p_src.x(), p_src.y());
                if (z_s < min_border_z) min_border_z = z_s;
                if (vz < min_border_z) min_border_z = vz;
            }
            curr = curr->next();
        } while (curr != ccb);

        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            do {
                auto twin_face = h_curr->twin()->face();
                if (twin_face->is_unbounded() || twin_face->number_of_surfaces() == 0) {
                    auto p_src = h_curr->source()->point();
                    auto p_tgt = h_curr->target()->point();
                    FT z_s = get_z(orig_f_idx, p_src.x(), p_src.y());
                    FT z_t = get_z(orig_f_idx, p_tgt.x(), p_tgt.y());
                    if (z_s < min_border_z) min_border_z = z_s;
                    if (z_t < min_border_z) min_border_z = z_t;
                }
                h_curr = h_curr->next();
            } while (h_curr != h_start);
        }
    }

    TideParams tide = override_tide;
    if (!tide.enabled && padding > FT(0)) {
        FT rot_u_min = 1000000, rot_u_max = -1000000;
        FT rot_v_min = 1000000, rot_v_max = -1000000;
        FT rot_z_min = 1000000, rot_z_max = -1000000;

        for (auto v : mesh_part.vertices()) {
            auto p_rot = to_z(mesh_part.point(v));
            FT rx = p_rot.x().exact();
            FT ry = p_rot.y().exact();
            FT rz = p_rot.z().exact();
            if (rx < rot_u_min) rot_u_min = rx;
            if (rx > rot_u_max) rot_u_max = rx;
            if (ry < rot_v_min) rot_v_min = ry;
            if (ry > rot_v_max) rot_v_max = ry;
            if (rz < rot_z_min) rot_z_min = rz;
            if (rz > rot_z_max) rot_z_max = rz;
        }

        FT center_u = (rot_u_min + rot_u_max) / FT(2);
        FT center_v = (rot_v_min + rot_v_max) / FT(2);
        FT half_span_u = (rot_u_max - rot_u_min) / FT(2);
        FT half_span_v = (rot_v_max - rot_v_min) / FT(2);
        FT half_span_z = (rot_z_max - rot_z_min) / FT(2);
        FT max_half = (half_span_u > half_span_v) ? ((half_span_u > half_span_z) ? half_span_u : half_span_z) : ((half_span_v > half_span_z) ? half_span_v : half_span_z);
        FT R = max_half + padding + FT(20);

        tide.enabled = true;
        tide.u_min = center_u - R;
        tide.u_max = center_u + R;
        tide.v_min = center_v - R;
        tide.v_max = center_v + R;
        FT mid_z = (rot_z_min + rot_z_max) / FT(2);
        tide.z_margin = (min_border_z < mid_z) ? min_border_z : mid_z;
        tide.z_top = rot_z_max + padding + FT(50);
    } else if (tide.enabled && min_border_z < tide.z_margin) {
        tide.z_margin = min_border_z;
    }

    FT h_ceiling_rot = tide.enabled ? tide.z_top : (max_vz_rot + FT(50));

    auto get_vertical_drop = [&](const CDT_Kernel::Point_2& p1, const CDT_Kernel::Point_2& p2) -> std::optional<FT> {
        std::optional<FT> best_min_z;
        for (const auto& vseg : vertical_segments) {
            if (CGAL::collinear(vseg.p_a, vseg.p_b, p1) && CGAL::collinear(vseg.p_a, vseg.p_b, p2)) {
                bool p1_in = (p1 == vseg.p_a || p1 == vseg.p_b || CGAL::collinear_are_ordered_along_line(vseg.p_a, p1, vseg.p_b));
                bool p2_in = (p2 == vseg.p_a || p2 == vseg.p_b || CGAL::collinear_are_ordered_along_line(vseg.p_a, p2, vseg.p_b));
                if (p1_in && p2_in) {
                    if (!best_min_z.has_value() || vseg.min_z < *best_min_z) {
                        best_min_z = vseg.min_z;
                    }
                }
            }
        }
        return best_min_z;
    };

    // Delegate solid wedge extrusion, solid-aware soup repair, and world-space transformation
    return construct_envelope_wedge(max_diag, get_z, h_ceiling_rot, from_z, to_z, tide, get_vertical_drop);
}

} // namespace mold
} // namespace geo
} // namespace jotcad
