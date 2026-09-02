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

} // namespace fix
} // namespace geo
} // namespace jotcad
