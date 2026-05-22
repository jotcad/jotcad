#ifndef PACKAIDE_TYPES_H
#define PACKAIDE_TYPES_H

#include <vector>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Aff_transformation_2.h>
#include <CGAL/Polygon_set_2.h>

namespace pack {
namespace packaide {

using K = CGAL::Exact_predicates_exact_constructions_kernel;
using FT = K::FT;
using Point_2 = CGAL::Point_2<K>;
using Vector_2 = CGAL::Vector_2<K>;
using Polygon_2 = CGAL::Polygon_2<K>;
using Polygon_with_holes_2 = CGAL::Polygon_with_holes_2<K>;
using Transformation = CGAL::Aff_transformation_2<K>;
using Polygon_set_2 = CGAL::Polygon_set_2<K>;

struct Sheet {
    Polygon_set_2 material;

    // Helper for rectangular creation
    static Sheet rectangle(FT w, FT h) {
        Polygon_2 rect;
        rect.push_back(Point_2(0, 0));
        rect.push_back(Point_2(w, 0));
        rect.push_back(Point_2(w, h));
        rect.push_back(Point_2(0, h));
        
        Sheet s;
        s.material.insert(rect);
        return s;
    }
};

inline bool is_good_polygon(const Polygon_2& p) {
    return !p.is_empty() && p.size() >= 3 && p.is_simple();
}

struct Placement {
    size_t part_index;
    Point_2 translation;
    double rotation; // radians
};

} // namespace packaide
} // namespace pack

#endif
