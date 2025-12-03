#include <iostream>
#include <vector>
#include <queue>
#include <limits>
#include <list>
#include <variant> // For std::get_if

// CGAL Includes
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/intersections.h>
#include <CGAL/Boolean_set_operations_2.h>

// 1. Define the Kernel
// EPECK is crucial here. Constructing new intersection points 
// requires exact arithmetic to prevent self-intersections due to rounding.
typedef CGAL::Exact_predicates_exact_constructions_kernel K;
typedef K::Point_2 Point_2;
typedef K::Segment_2 Segment_2;
typedef K::Line_2 Line_2;
typedef CGAL::Polygon_2<K> Polygon_2;

// Structure to track potential edge removals
enum CollapseType {
    CONVEX_EXTEND,
    CONCAVE_COLLAPSE
};

struct EdgeCollapse {
    std::list<Point_2>::iterator v_start; // Iterator to v_i
    double area_added;
    Point_2 new_intersection;
    bool valid; // Flag to invalidate if neighbors change
    CollapseType type; // New field

    // Priority Queue comparator (Min-Heap based on area)
    bool operator>(const EdgeCollapse& other) const {
        return area_added > other.area_added;
    }
};

class SubsumingSimplifier {
public:
    Polygon_2 simplify(const Polygon_2& input_poly, std::size_t target_vertices) {
        std::list<Point_2> current_points(input_poly.vertices_begin(), input_poly.vertices_end());
        Polygon_2 current_poly = input_poly;

        std::cout << "Starting simplification with " << current_points.size() << " vertices, targeting " << target_vertices << " vertices." << std::endl;

        while (current_points.size() > target_vertices) {
            
            std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, std::greater<EdgeCollapse>> queue;
            
            auto it = current_points.begin();
            for (size_t i = 0; i < current_points.size(); ++i) {
                EdgeCollapse collapse = evaluate_collapse(current_points, it, current_poly);
                if (collapse.valid) {
                    queue.push(collapse);
                }
                it++;
            }

            std::cout << "  Found " << queue.size() << " valid collapse candidates." << std::endl;

            if (queue.empty()) {
                std::cout << "No valid collapses remaining. Breaking simplification loop." << std::endl;
                break;
            }

            EdgeCollapse best = queue.top();
            std::cout << "  Applying best collapse, replacing "
                      << CGAL::to_double((*best.v_start).x()) << "," << CGAL::to_double((*best.v_start).y())
                      << " and "
                      << CGAL::to_double((*next_cyclic(current_points, best.v_start)).x()) << "," << CGAL::to_double((*next_cyclic(current_points, best.v_start)).y())
                      << " with "
                      << CGAL::to_double(best.new_intersection.x()) << "," << CGAL::to_double(best.new_intersection.y())
                      << ". Area added: " << best.area_added << std::endl;
            apply_collapse(current_points, best);
            
            current_poly.clear();
            for (const auto& p : current_points) current_poly.push_back(p);

            std::cout << "  Current vertices after collapse: " << current_points.size() << std::endl;
        }

        std::cout << "Simplification finished. Resulting in " << current_points.size() << " vertices." << std::endl;
        return current_poly;
    }

private:
    // Helper to get cyclic iterator
    template <typename Container, typename Iterator>
    Iterator next_cyclic(Container& c, Iterator it) {
        auto next = std::next(it);
        return (next == c.end()) ? c.begin() : next;
    }

    template <typename Container, typename Iterator>
    Iterator prev_cyclic(Container& c, Iterator it) {
        return (it == c.begin()) ? std::prev(c.end()) : std::prev(it);
    }

    // Evaluate the cost of removing edge starting at 'it' (removing it and it->next)
    EdgeCollapse evaluate_collapse(std::list<Point_2>& points, std::list<Point_2>::iterator it_curr, const Polygon_2& poly_context) {
        EdgeCollapse candidate;
        candidate.valid = false;
        candidate.v_start = it_curr;

        auto it_B = it_curr;
        auto it_C = next_cyclic(points, it_B);
        auto it_A = prev_cyclic(points, it_B);
        auto it_D = next_cyclic(points, it_C);

        Point_2 A = *it_A;
        Point_2 B = *it_B;
        Point_2 C = *it_C;
        Point_2 D = *it_D;
        
        std::cout << "  Evaluating edge " << CGAL::to_double(B.x()) << "," << CGAL::to_double(B.y()) << " -> " << CGAL::to_double(C.x()) << "," << CGAL::to_double(C.y()) << std::endl;
        std::cout << "  (Chain A=" << CGAL::to_double(A.x()) << "," << CGAL::to_double(A.y())
                  << " B=" << CGAL::to_double(B.x()) << "," << CGAL::to_double(B.y())
                  << " C=" << CGAL::to_double(C.x()) << "," << CGAL::to_double(C.y())
                  << " D=" << CGAL::to_double(D.x()) << "," << CGAL::to_double(D.y()) << ")" << std::endl;

        // Instrument convexity/concavity of B
        auto orientation_B = CGAL::orientation(A, B, C);

        if (orientation_B == CGAL::LEFT_TURN) {
            std::cout << "  Corner B is convex (Left Turn)" << std::endl;
            candidate.type = CONVEX_EXTEND;

            Line_2 line_AB(A, B);
            Line_2 line_DC(D, C);

            const auto result = CGAL::intersection(line_AB, line_DC);
            
            if (result) {
                if (const Point_2* p_point = std::get_if<Point_2>(&*result)) {
                    candidate.new_intersection = *p_point;
                    std::cout << "  Intersection found at P=" << CGAL::to_double(p_point->x()) << "," << CGAL::to_double(p_point->y()) << std::endl;

                    if (CGAL::scalar_product(B - A, candidate.new_intersection - B) < 0) {
                        std::cout << "  Directionality check failed (B->P backward from A->B)" << std::endl;
                        return candidate;
                    }
                    if (CGAL::scalar_product(C - D, candidate.new_intersection - C) < 0) {
                        std::cout << "  Directionality check failed (C->P backward from D->C)" << std::endl;
                        return candidate;
                    }
                    
                    if (CGAL::orientation(A, B, candidate.new_intersection) == CGAL::RIGHT_TURN) {
                        std::cout << "  Subsuming check failed (A,B,P is right turn)" << std::endl;
                        return candidate;
                    }
                    
                    candidate.area_added = CGAL::to_double(CGAL::area(B, candidate.new_intersection, C));
                    std::cout << "  Area added: " << candidate.area_added << std::endl;

                    if (check_intersection_safety(points, A, B, C, D, candidate.new_intersection, candidate.type)) {
                        candidate.valid = true;
                        std::cout << "  Global validity check passed." << std::endl;
                    } else {
                        std::cout << "  Global validity check failed." << std::endl;
                    }
                } else {
                    if (const Line_2* p_line = std::get_if<Line_2>(&*result)) {
                        std::cout << "  Intersection found but not a Point_2. Type: Line_2" << std::endl;
                    } else {
                        std::cout << "  Intersection found but not a Point_2. Type: Unknown" << std::endl;
                    }
                }
            } else {
                std::cout << "  No intersection found for lines AB and DC." << std::endl;
            }
        } else if (orientation_B == CGAL::RIGHT_TURN) {
            std::cout << "  Corner B is concave (Right Turn)" << std::endl;
            candidate.type = CONCAVE_COLLAPSE;
            candidate.new_intersection = C; // When collapsing, effectively A connects to C

            candidate.area_added = -CGAL::to_double(CGAL::area(A, B, C)); // Negative area to prioritize removal
            std::cout << "  Area to be removed: " << -candidate.area_added << std::endl;

            if (check_intersection_safety(points, A, B, C, D, candidate.new_intersection, candidate.type)) {
                candidate.valid = true;
                std::cout << "  Global validity check passed for concave collapse." << std::endl;
            } else {
                std::cout << "  Global validity check failed for concave collapse." << std::endl;
            }
        } else { // Collinear
            std::cout << "  Corner B is collinear - not a valid collapse candidate." << std::endl;
        }

        return candidate;
    }

    bool check_intersection_safety(std::list<Point_2>& points, Point_2 A, Point_2 B, Point_2 C, Point_2 D, Point_2 P, CollapseType type) {
        if (type == CONCAVE_COLLAPSE) {
            // For concave collapse, the new edge is A-C
            Segment_2 new_edge(A, C);

            for (auto it = points.begin(); it != points.end(); ++it) {
                Point_2 u = *it;
                Point_2 v = *next_cyclic(points, it);

                // Skip the edges that are part of the collapse (A-B, B-C) and the new edge A-C
                if ( (u == A && v == B) || (u == B && v == A) ||
                     (u == B && v == C) || (u == C && v == B) ||
                     (u == A && v == C) || (u == C && v == A) ) {
                    continue;
                }

                Segment_2 other_edge(u, v);
                const auto result = CGAL::intersection(new_edge, other_edge);
                if (result) {
                    if (const Point_2* p_int = std::get_if<Point_2>(&*result)) {
                        // Intersection is allowed only at endpoints A or C
                        if (*p_int != A && *p_int != C) {
                            return false;
                        }
                    } else {
                        // Intersection is a segment, which is bad.
                        return false;
                    }
                }
            }
            return true;
        } else { // CONVEX_EXTEND
            // Construct the two new segments for convex extension
            Segment_2 seg1(A, P);
            Segment_2 seg2(P, D);

            for (auto it = points.begin(); it != points.end(); ++it) {
                Point_2 u = *it;
                Point_2 v = *next_cyclic(points, it);
                
                // Skip the edges that are being removed or modified (A-B, B-C, C-D)
                if ( (u == A && v == B) || (u == B && v == A) ||
                     (u == B && v == C) || (u == C && v == B) ||
                     (u == C && v == D) || (u == D && v == C) ) {
                    continue;
                }

                Segment_2 other_edge(u, v);
                auto result = CGAL::intersection(seg1, other_edge);
                if (result) {
                    if (const Point_2* p_int = std::get_if<Point_2>(&*result)) {
                        if (*p_int != A) { // Only intersection allowed is at A
                            return false;
                        }
                    } else {
                        return false;
                    }
                }

                result = CGAL::intersection(seg2, other_edge);
                if (result) {
                    if (const Point_2* p_int = std::get_if<Point_2>(&*result)) {
                        if (*p_int != D) { // Only intersection allowed is at D
                            return false;
                        }
                    } else {
                        return false;
                    }
                }
            }
            return true;
        }
    }

    void apply_collapse(std::list<Point_2>& points, const EdgeCollapse& collapse) {
        if (collapse.type == CONVEX_EXTEND) {
            auto it_B = collapse.v_start;
            auto it_C = next_cyclic(points, it_B);

            // Insert new point before B
            points.insert(it_B, collapse.new_intersection);

            // Remove B and C
            points.erase(it_B); // Erases the original B
            points.erase(it_C); // Erases the original C
        } else if (collapse.type == CONCAVE_COLLAPSE) {
            // For concave collapse, we remove vertex B.
            // collapse.v_start points to B.
            points.erase(collapse.v_start);
        }
    }
};

namespace test {

void SimplifyCutCorner() {
    // Example: A box with a "dent" or cut corner
    Polygon_2 poly;
    poly.push_back(Point_2(0, 0));
    poly.push_back(Point_2(10, 0));
    poly.push_back(Point_2(10, 8));
    poly.push_back(Point_2(9, 9));  // Cut corner 1
    poly.push_back(Point_2(8, 10)); // Cut corner 2
    poly.push_back(Point_2(0, 10));

    std::cout << "--- Test: SimplifyCutCorner ---" << std::endl;
    std::cout << "Original Vertices: " << poly.size() << std::endl;
    std::cout << "Original Area: " << CGAL::to_double(poly.area()) << std::endl;

    SubsumingSimplifier sim;
    // Reduce to 4 vertices (should reconstruct the bounding box corner)
    Polygon_2 simplified = sim.simplify(poly, 4);

    std::cout << "Simplified Vertices: " << simplified.size() << std::endl;
    std::cout << "Simplified Area: " << CGAL::to_double(simplified.area()) << std::endl;
    
    std::cout << "Resulting Polygon:" << std::endl;
    for (auto v = simplified.vertices_begin(); v != simplified.vertices_end(); ++v) {
        std::cout << "(" << CGAL::to_double(v->x()) << ", " << CGAL::to_double(v->y()) << ")" << std::endl;
    }
    assert(simplified.size() == 4);
    assert(CGAL::to_double(simplified.area()) > 99.0); // Should be 100
    std::cout << "---------------------------------" << std::endl;
}

void SimplifyComplexDent() {
    // Example: A box with a more complex "dent"
    Polygon_2 poly;
    poly.push_back(Point_2(0, 0));
    poly.push_back(Point_2(10, 0));
    poly.push_back(Point_2(10, 5));
    poly.push_back(Point_2(9, 5));
    poly.push_back(Point_2(9, 6));
    poly.push_back(Point_2(8, 6));
    poly.push_back(Point_2(8, 7));
    poly.push_back(Point_2(7, 7));
    poly.push_back(Point_2(7, 8));
    poly.push_back(Point_2(6, 8));
    poly.push_back(Point_2(6, 9));
    poly.push_back(Point_2(5, 9));
    poly.push_back(Point_2(5, 10));
    poly.push_back(Point_2(0, 10));

    std::cout << "--- Test: SimplifyComplexDent ---" << std::endl;
    std::cout << "Original Vertices: " << poly.size() << std::endl;
    std::cout << "Original Area: " << CGAL::to_double(poly.area()) << std::endl;

    SubsumingSimplifier sim;
    // Reduce to 4 vertices (should reconstruct the bounding box)
    Polygon_2 simplified = sim.simplify(poly, 4);

    std::cout << "Simplified Vertices: " << simplified.size() << std::endl;
    std::cout << "Simplified Area: " << CGAL::to_double(simplified.area()) << std::endl;
    
    std::cout << "Resulting Polygon:" << std::endl;
    for (auto v = simplified.vertices_begin(); v != simplified.vertices_end(); ++v) {
        std::cout << "(" << CGAL::to_double(v->x()) << ", " << CGAL::to_double(v->y()) << ")" << std::endl;
    }
    assert(simplified.size() == 4);
    assert(CGAL::to_double(simplified.area()) > 99.0); // Should be 100
    std::cout << "-----------------------------------" << std::endl;
}

void TestLineIntersection() {
    std::cout << "--- Test: TestLineIntersection ---" << std::endl;
    
    // Test Case 1: Simple intersection
    Line_2 l1(Point_2(0,0), Point_2(10,10)); // y = x
    Line_2 l2(Point_2(0,10), Point_2(10,0)); // y = -x + 10
    const auto result1 = CGAL::intersection(l1, l2);
    const Point_2* p1 = std::get_if<Point_2>(&*result1);
    assert(p1);
    assert(CGAL::to_double(p1->x()) == 5.0 && CGAL::to_double(p1->y()) == 5.0);
    std::cout << "  Simple intersection passed." << std::endl;

    // Test Case 2: Parallel lines (no intersection)
    Line_2 l3(Point_2(0,0), Point_2(10,0)); // y = 0
    Line_2 l4(Point_2(0,1), Point_2(10,1)); // y = 1
    const auto result2 = CGAL::intersection(l3, l4);
    assert(!result2);
    std::cout << "  Parallel lines passed." << std::endl;

    // Test Case 3: Collinear lines (should be Line_2 as lines are infinite)
    Line_2 l5(Point_2(0,0), Point_2(1,0)); // x-axis
    Line_2 l6(Point_2(2,0), Point_2(3,0)); // x-axis, separated but define the same infinite line
    const auto result3 = CGAL::intersection(l5, l6);
    const Line_2* l3_res = std::get_if<Line_2>(&*result3);
    assert(l3_res);
    std::cout << "  Collinear lines (Line_2) passed." << std::endl;

    // Test Case 4: Collinear lines (overlap - should be Line_2 as lines are infinite)
    Line_2 l7(Point_2(0,0), Point_2(1,0)); // x-axis
    Line_2 l8(Point_2(-1,0), Point_2(10,0)); // x-axis, overlapping
    const auto result4 = CGAL::intersection(l7, l8);
    const Line_2* l4_res = std::get_if<Line_2>(&*result4);
    assert(l4_res);
    std::cout << "  Collinear lines (overlap - Line_2) passed." << std::endl;

    // Test Case 5: Intersection at an endpoint (simple case)
    Line_2 l9(Point_2(0,0), Point_2(10,0));
    Line_2 l10(Point_2(10,0), Point_2(10,10));
    const auto result5 = CGAL::intersection(l9, l10);
    const Point_2* p5 = std::get_if<Point_2>(&*result5);
    assert(p5);
    assert(CGAL::to_double(p5->x()) == 10.0 && CGAL::to_double(p5->y()) == 0.0);
    std::cout << "  Intersection at endpoint passed." << std::endl;

    std::cout << "-----------------------------------" << std::endl;
}

void SimplifyConcaveTurn() {
    // Example: A polygon with a concave turn to be collapsed
    Polygon_2 poly;
    poly.push_back(Point_2(0, 0));
    poly.push_back(Point_2(10, 0));
    poly.push_back(Point_2(10, 10));
    poly.push_back(Point_2(5, 5));  // Concave turn here
    poly.push_back(Point_2(0, 10));

    std::cout << "--- Test: SimplifyConcaveTurn ---" << std::endl;
    std::cout << "Original Vertices: " << poly.size() << std::endl;
    std::cout << "Original Area: " << CGAL::to_double(poly.area()) << std::endl;

    SubsumingSimplifier sim;
    // Reduce to 4 vertices (should collapse the concave turn at (5,5))
    Polygon_2 simplified = sim.simplify(poly, 4);

    std::cout << "Simplified Vertices: " << simplified.size() << std::endl;
    std::cout << "Simplified Area: " << CGAL::to_double(simplified.area()) << std::endl;
    
    std::cout << "Resulting Polygon:" << std::endl;
    for (auto v = simplified.vertices_begin(); v != simplified.vertices_end(); ++v) {
        std::cout << "(" << CGAL::to_double(v->x()) << ", " << CGAL::to_double(v->y()) << ")" << std::endl;
    }
    assert(simplified.size() == 4);
    // The expected area after removing (5,5) and connecting (10,10) to (0,10)
    // is the area of the bounding box (10x10) = 100.
    // Original area = Area of rect (0,0)-(10,10) - Area of triangle (10,10)-(5,5)-(0,10)
    // Area of triangle (10,10)-(5,5)-(0,10) = 0.5 * base * height = 0.5 * 10 * 5 = 25
    // Original area = 100 - 25 = 75
    // After collapsing (5,5), it should become a 10x10 square.
    assert(CGAL::to_double(simplified.area()) > 99.0); 
    std::cout << "-----------------------------------" << std::endl;
}

} // namespace test

int main(int argc, char** argv) {
    test::TestLineIntersection(); // Call the new line intersection test
    test::SimplifyCutCorner();
    test::SimplifyComplexDent();
    test::SimplifyConcaveTurn();
    return 0;
}