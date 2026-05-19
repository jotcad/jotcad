#ifndef PACKAIDE_CORE_H
#define PACKAIDE_CORE_H

#include <CGAL/minkowski_sum_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include "packaide_types.h"

namespace pack {
namespace packaide {

/**
 * @brief Creates a square polygon centered at (0,0) with given side length.
 */
inline Polygon_2 create_offset_square(FT d) {
    Polygon_2 square;
    FT r = d / FT(2.0);
    square.push_back(Point_2(-r, -r));
    square.push_back(Point_2(r, -r));
    square.push_back(Point_2(r, r));
    square.push_back(Point_2(-r, r));
    return square;
}

/**
 * @brief Computes the No-Fit Polygon (NFP) of B with respect to A.
 */
inline Polygon_with_holes_2 compute_nfp(const Polygon_with_holes_2& poly_A, const Polygon_with_holes_2& poly_B, FT spacing = FT(0)) {
    Transformation scale(CGAL::SCALING, -1);
    Polygon_with_holes_2 minus_B = CGAL::transform(scale, poly_B);
    
    // spacing is currently handled via external offsetting or symbolic epsilon if needed
    return CGAL::minkowski_sum_2(poly_A, minus_B);
}

/**
 * @brief Computes the Inner-Fit Polygon (IFP) of B within a Polygon with holes A.
 * This is effectively the Minkowski Difference (A - B).
 */
inline Polygon_set_2 compute_ifp_pwh(const Polygon_with_holes_2& poly_A, const Polygon_with_holes_2& poly_B) {
    // For a general PWH, the IFP is:
    // IFP(A_outer, B) - Union(NFP(A_hole_i, B))
    
    // 1. Compute IFP of B within outer boundary of A
    auto bb_A = poly_A.outer_boundary().bbox();
    auto bb_B = poly_B.outer_boundary().bbox();
    
    FT w_A = FT(bb_A.xmax() - bb_A.xmin());
    FT h_A = FT(bb_A.ymax() - bb_A.ymin());
    FT w_B = FT(bb_B.xmax() - bb_B.xmin());
    FT h_B = FT(bb_B.ymax() - bb_B.ymin());

    if (w_B > w_A || h_B > h_A) return Polygon_set_2();

    // For now, we assume rectangular outer boundaries for sheets, 
    // centered at their own bounding boxes for the IFP calculation.
    FT dx = w_A - w_B;
    FT dy = h_A - h_B;
    
    if (dx < FT(0) || dy < FT(0)) return Polygon_set_2();

    // If dx and dy are both 0, the feasible region is a single point.
    // However, Polygon_set_2 requires 2D regions. We handle point-fit in the engine.
    if (dx == FT(0) || dy == FT(0)) return Polygon_set_2();

    Polygon_2 rect_ifp;
    rect_ifp.push_back(Point_2(bb_A.xmin(), bb_A.ymin()));
    rect_ifp.push_back(Point_2(bb_A.xmin() + dx, bb_A.ymin()));
    rect_ifp.push_back(Point_2(bb_A.xmin() + dx, bb_A.ymin() + dy));
    rect_ifp.push_back(Point_2(bb_A.xmin(), bb_A.ymin() + dy));

    if (!is_good_polygon(rect_ifp)) return Polygon_set_2();
    
    Polygon_set_2 result(rect_ifp);

    // 2. Subtract NFPs of all holes in A
    for (auto hit = poly_A.holes_begin(); hit != poly_A.holes_end(); ++hit) {
        Polygon_with_holes_2 hole_pwh(*hit);
        auto nfp = compute_nfp(hole_pwh, poly_B);
        result.difference(nfp);
    }

    return result;
}


struct BoundingBoxHeuristic {
    FT xmin, ymin, xmax, ymax;
    bool initialized = false;

    BoundingBoxHeuristic() {}

    void add(const Polygon_with_holes_2& p) {
        auto bb = p.outer_boundary().bbox();
        FT p_xmin = FT(bb.xmin());
        FT p_ymin = FT(bb.ymin());
        FT p_xmax = FT(bb.xmax());
        FT p_ymax = FT(bb.ymax());

        if (!initialized) {
            xmin = p_xmin; ymin = p_ymin; xmax = p_xmax; ymax = p_ymax;
            initialized = true;
        } else {
            xmin = std::min(xmin, p_xmin);
            ymin = std::min(ymin, p_ymin);
            xmax = std::max(xmax, p_xmax);
            ymax = std::max(ymax, p_ymax);
        }
    }

    FT score_after_adding(const Polygon_with_holes_2& p) const {
        auto bb = p.outer_boundary().bbox();
        FT p_xmin = FT(bb.xmin());
        FT p_ymin = FT(bb.ymin());
        FT p_xmax = FT(bb.xmax());
        FT p_ymax = FT(bb.ymax());

        if (!initialized) {
            return (p_xmax - p_xmin) * (p_ymax - p_ymin);
        }

        FT cur_xmin = std::min(xmin, p_xmin);
        FT cur_ymin = std::min(ymin, p_ymin);
        FT cur_xmax = std::max(xmax, p_xmax);
        FT cur_ymax = std::max(ymax, p_ymax);
        
        return (cur_xmax - cur_xmin) * (cur_ymax - cur_ymin);
    }
};

} // namespace packaide
} // namespace pack

#endif
