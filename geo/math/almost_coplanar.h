#pragma once
#include "matrix.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <cmath>

namespace jotcad {
namespace geo {

// Default dihedral cosine threshold for almost-coplanar facets: cos(pi - 0.001 rad) ≈ -0.999999
constexpr double DEFAULT_ALMOST_COPLANAR_COS_THRESHOLD = -0.999999;

/**
 * is_almost_coplanar_edge:
 * Checks whether two neighboring triangles sharing edge (p, q) with opposite vertices r and s
 * are almost coplanar using CGAL's exact dihedral angle comparison predicate.
 * 
 * @param p Source vertex of shared edge
 * @param q Target vertex of shared edge
 * @param r Opposite vertex of face 1
 * @param s Opposite vertex of face 2
 * @param cos_threshold Cosine of maximum allowable dihedral angle (default: 0.999999)
 */
template <typename Kernel = EK>
inline bool is_almost_coplanar_edge(const typename Kernel::Point_3& p,
                                    const typename Kernel::Point_3& q,
                                    const typename Kernel::Point_3& r,
                                    const typename Kernel::Point_3& s,
                                    double cos_threshold = DEFAULT_ALMOST_COPLANAR_COS_THRESHOLD) {
    typename Kernel::Compare_dihedral_angle_3 pred;
    return pred(p, q, r, s, typename Kernel::FT(cos_threshold)) == CGAL::LARGER;
}

/**
 * are_almost_coplanar_normals:
 * Checks whether two 3D normal vectors are almost parallel within the cosine threshold.
 */
template <typename Kernel = EK>
inline bool are_almost_coplanar_normals(const typename Kernel::Vector_3& n1,
                                        const typename Kernel::Vector_3& n2,
                                        double cos_threshold = DEFAULT_ALMOST_COPLANAR_COS_THRESHOLD) {
    auto len1_sq = n1.squared_length();
    auto len2_sq = n2.squared_length();
    if (len1_sq == 0 || len2_sq == 0) return true;
    
    // Normals must point in the same direction
    auto dot = n1 * n2;
    if (dot <= 0) return false;
    
    double dot_d = CGAL::to_double(dot);
    double l1_d = std::sqrt(CGAL::to_double(len1_sq));
    double l2_d = std::sqrt(CGAL::to_double(len2_sq));
    return (dot_d / (l1_d * l2_d)) >= cos_threshold;
}

} // namespace geo
} // namespace jotcad
