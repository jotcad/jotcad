#pragma once
#include "kernel.h"
#include <CGAL/intersections.h>

namespace jotcad {
namespace geo {
namespace fix {

/**
 * is_point_on_segment:
 * Returns true if point P lies strictly in the interior of segment (A, B).
 * Exact rational predicate in pure EK::FT using CGAL native do_intersect.
 */
template <typename K = EK>
inline bool is_point_on_segment(
    const typename K::Point_3& P, 
    const typename K::Point_3& A, 
    const typename K::Point_3& B
) {
    if (P == A || P == B) return false;
    typename K::Segment_3 seg(A, B);
    return CGAL::do_intersect(P, seg);
}

/**
 * is_point_in_triangle:
 * Returns true if point P lies strictly in the 2D interior of triangle (A, B, C).
 * Exact rational predicate in pure EK::FT using CGAL native do_intersect.
 */
template <typename K = EK>
inline bool is_point_in_triangle(
    const typename K::Point_3& P, 
    const typename K::Point_3& A, 
    const typename K::Point_3& B, 
    const typename K::Point_3& C
) {
    if (P == A || P == B || P == C) return false;
    if (is_point_on_segment<K>(P, A, B) || is_point_on_segment<K>(P, B, C) || is_point_on_segment<K>(P, C, A)) return false;
    typename K::Triangle_3 tri(A, B, C);
    return CGAL::do_intersect(P, tri);
}

/**
 * segment_triangle_intersection:
 * Computes exact rational intersection between segment [A, B] and triangle (C, D, E).
 * Returns:
 *   - 1: intersection is a 1D segment [out_p1, out_p2] across the face interior
 *   - 0: intersection is a 0D point out_p1 on the face interior
 *   - -1: no interior intersection (disjoint or purely on triangle boundary)
 */
template <typename K = EK>
inline int segment_triangle_intersection(
    const typename K::Point_3& A,
    const typename K::Point_3& B,
    const typename K::Point_3& C,
    const typename K::Point_3& D,
    const typename K::Point_3& E,
    typename K::Point_3& out_p1,
    typename K::Point_3& out_p2
) {
    typename K::Segment_3 seg(A, B);
    typename K::Triangle_3 tri(C, D, E);
    auto inter = CGAL::intersection(seg, tri);
    if (!inter) return -1;

    if (const typename K::Segment_3* s = std::get_if<typename K::Segment_3>(&*inter)) {
        out_p1 = s->source();
        out_p2 = s->target();
        if (out_p1 == out_p2) {
            if (out_p1 == C || out_p1 == D || out_p1 == E) return -1;
            return 0;
        }
        bool p1_is_vert = (out_p1 == C || out_p1 == D || out_p1 == E);
        bool p2_is_vert = (out_p2 == C || out_p2 == D || out_p2 == E);
        if (p1_is_vert && p2_is_vert) return -1; // Exact triangle boundary edge
        return 1;
    } else if (const typename K::Point_3* p = std::get_if<typename K::Point_3>(&*inter)) {
        out_p1 = *p;
        if (out_p1 == C || out_p1 == D || out_p1 == E) return -1;
        return 0;
    }
    return -1;
}

/**
 * is_point_near_plane:
 * Checks if point V is within distance threshold from plane (P0, N0):
 * ((V - P0) . N0)^2 <= max_dist_sq * (N0 . N0)
 * Pure exact rational arithmetic in EK::FT.
 */
template <typename K = EK>
inline bool is_point_near_plane(
    const typename K::Point_3& V,
    const typename K::Point_3& P0,
    const typename K::Vector_3& N0,
    const typename K::FT& max_dist_sq
) {
    auto n_sq = N0.squared_length();
    if (n_sq == 0) return true;
    auto dot = (V - P0) * N0;
    return (dot * dot) <= (max_dist_sq * n_sq);
}

/**
 * are_near_parallel_normals:
 * Checks if normal vectors N1 and N2 are aligned within angular sine-squared threshold:
 * |N1 x N2|^2 <= max_sin_sq * |N1|^2 * |N2|^2 and N1 . N2 > 0
 * Pure exact rational arithmetic in EK::FT.
 */
template <typename K = EK>
inline bool are_near_parallel_normals(
    const typename K::Vector_3& N1,
    const typename K::Vector_3& N2,
    const typename K::FT& max_sin_sq
) {
    auto len1_sq = N1.squared_length();
    auto len2_sq = N2.squared_length();
    if (len1_sq == 0 || len2_sq == 0) return true;
    if (N1 * N2 <= 0) return false;
    auto cross = CGAL::cross_product(N1, N2);
    return (cross.squared_length() <= max_sin_sq * len1_sq * len2_sq);
}

/**
 * do_triangles_form_convex_quad:
 * Checks whether two neighboring triangles sharing edge (A, B) with opposite vertices C and D
 * form a strictly convex quadrilateral (i.e. diagonals AB and CD intersect).
 */
template <typename K = EK>
inline bool do_triangles_form_convex_quad(
    const typename K::Point_3& A,
    const typename K::Point_3& B,
    const typename K::Point_3& C,
    const typename K::Point_3& D
) {
    typename K::Segment_3 seg_AB(A, B);
    typename K::Segment_3 seg_CD(C, D);
    return CGAL::do_intersect(seg_AB, seg_CD);
}

} // namespace fix
} // namespace geo
} // namespace jotcad
