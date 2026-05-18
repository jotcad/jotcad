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
 * @brief Computes the Inner-Fit Polygon (IFP) of B within a rectangular Sheet A.
 */
inline Polygon_2 compute_ifp(const Sheet& sheet, const Polygon_with_holes_2& poly_B) {
    auto bb = poly_B.outer_boundary().bbox();
    FT w_B = FT(bb.xmax() - bb.xmin());
    FT h_B = FT(bb.ymax() - bb.ymin());

    if (w_B > sheet.width || h_B > sheet.height) {
        return Polygon_2();
    }

    FT dx = sheet.width - w_B;
    FT dy = sheet.height - h_B;

    // Protocol: compute_ifp MUST only return a 2D region for use with Polygon_set_2.
    // If dx or dy is 0, the feasible region is a line or point, which is handled 
    // as a special case in the engine.
    if (dx == FT(0) || dy == FT(0)) {
        return Polygon_2();
    }

    Polygon_2 ifp;
    ifp.push_back(Point_2(0, 0));
    ifp.push_back(Point_2(dx, 0));
    ifp.push_back(Point_2(dx, dy));
    ifp.push_back(Point_2(0, dy));

    assert(is_good_polygon(ifp));

    return ifp;
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
