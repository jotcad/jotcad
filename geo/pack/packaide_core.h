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
inline Polygon_with_holes_2 compute_nfp(const Polygon_with_holes_2& poly_A, const Polygon_with_holes_2& poly_B) {
    Transformation scale(CGAL::SCALING, -1);
    Polygon_with_holes_2 minus_B = CGAL::transform(scale, poly_B);
    
    // CGAL's minkowski_sum_2 handles PWH + PWH via decomposition.
    return CGAL::minkowski_sum_2(poly_A, minus_B);
}

/**
 * @brief Computes the Inner-Fit Polygon (IFP) of B within a Polygon with holes A.
 * Uses the Exterior Subtraction method to handle arbitrary sheet shapes.
 */
inline Polygon_set_2 compute_ifp_pwh(const Polygon_with_holes_2& poly_A, const Polygon_with_holes_2& poly_B) {
    auto bb_A = poly_A.outer_boundary().bbox();
    auto bb_B = poly_B.outer_boundary().bbox();
    
    FT w_A = FT(bb_A.xmax() - bb_A.xmin());
    FT h_A = FT(bb_A.ymax() - bb_A.ymin());
    FT w_B = FT(bb_B.xmax() - bb_B.xmin());
    FT h_B = FT(bb_B.ymax() - bb_B.ymin());

    // 1. Initial Feasible Region: The bounding box of A shrunk by the bounding box of B.
    // This ensures the part B (normalized to its own BB min at 0,0) stays within the sheet's BB.
    FT dx = w_A - w_B;
    FT dy = h_A - h_B;
    if (dx < FT(0) || dy < FT(0)) return Polygon_set_2();

    Polygon_2 rect_initial;
    rect_initial.push_back(Point_2(bb_A.xmin(), bb_A.ymin()));
    rect_initial.push_back(Point_2(bb_A.xmin() + dx, bb_A.ymin()));
    rect_initial.push_back(Point_2(bb_A.xmin() + dx, bb_A.ymin() + dy));
    rect_initial.push_back(Point_2(bb_A.xmin(), bb_A.ymin() + dy));
    
    Polygon_set_2 result(rect_initial);

    // 2. Subtract the "Exterior" of the sheet.
    // We create a frame M that is larger than the sheet's BB.
    FT pad = std::max(w_B, h_B) * FT(2.0);
    Polygon_2 frame;
    frame.push_back(Point_2(bb_A.xmin() - pad, bb_A.ymin() - pad));
    frame.push_back(Point_2(bb_A.xmax() + pad, bb_A.ymin() - pad));
    frame.push_back(Point_2(bb_A.xmax() + pad, bb_A.ymax() + pad));
    frame.push_back(Point_2(bb_A.xmin() - pad, bb_A.ymax() + pad));
    
    Polygon_set_2 exterior(frame);
    exterior.difference(poly_A.outer_boundary()); // Now contains everything OUTSIDE the sheet boundary.

    std::vector<Polygon_with_holes_2> exterior_polys;
    exterior.polygons_with_holes(std::back_inserter(exterior_polys));
    
    for (const auto& p : exterior_polys) {
        auto nfp = compute_nfp(p, poly_B);
        result.difference(nfp);
    }

    // 3. Subtract NFPs of all actual internal holes in the sheet.
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
