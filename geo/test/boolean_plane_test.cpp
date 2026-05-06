#include "boolean/engine.h"
#include "test_base.h"

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

void test_cut_segments() {
    // Segment from (-10, 0, 0) to (10, 0, 0)
    std::vector<std::pair<EK::Point_3, EK::Point_3>> segments;
    segments.push_back({EK::Point_3(-10, 0, 0), EK::Point_3(10, 0, 0)});

    // Plane at X=0, normal pointing +X
    // Positive side is X > 0
    // Negative side is X < 0
    EK::Plane_3 plane(1, 0, 0, 0); 

    // Cut (Subtraction) keeps POSITIVE side (X > 0)
    Engine::cut_segments_by_plane(segments, plane);

    assert(segments.size() == 1);
    assert(segments[0].first == EK::Point_3(0, 0, 0));
    assert(segments[0].second == EK::Point_3(10, 0, 0));
    std::cout << "  ✅ CutSegmentsByPlane PASS" << std::endl;
}

void test_clip_segments() {
    std::vector<std::pair<EK::Point_3, EK::Point_3>> segments;
    segments.push_back({EK::Point_3(-10, 0, 0), EK::Point_3(10, 0, 0)});

    EK::Plane_3 plane(1, 0, 0, 0); 

    // Clip (Intersection) keeps NEGATIVE side (X < 0)
    Engine::clip_segments_by_plane(segments, plane);

    assert(segments.size() == 1);
    assert(segments[0].first == EK::Point_3(-10, 0, 0));
    assert(segments[0].second == EK::Point_3(0, 0, 0));
    std::cout << "  ✅ ClipSegmentsByPlane PASS" << std::endl;
}

void test_cut_points() {
    std::vector<EK::Point_3> points = {
        EK::Point_3(-5, 0, 0),
        EK::Point_3(5, 0, 0)
    };

    EK::Plane_3 plane(1, 0, 0, 0); 

    // Cut keeps POSITIVE side (X > 0)
    Engine::cut_points_by_plane(points, plane);

    assert(points.size() == 1);
    assert(points[0] == EK::Point_3(5, 0, 0));
    std::cout << "  ✅ CutPointsByPlane PASS" << std::endl;
}

void test_clip_points() {
    std::vector<EK::Point_3> points = {
        EK::Point_3(-5, 0, 0),
        EK::Point_3(5, 0, 0)
    };

    EK::Plane_3 plane(1, 0, 0, 0); 

    // Clip keeps NEGATIVE side (X < 0)
    Engine::clip_points_by_plane(points, plane);

    assert(points.size() == 1);
    assert(points[0] == EK::Point_3(-5, 0, 0));
    std::cout << "  ✅ ClipPointsByPlane PASS" << std::endl;
}

int main() {
    std::cout << "Testing Boolean Plane Operations..." << std::endl;
    test_cut_segments();
    test_clip_segments();
    test_cut_points();
    test_clip_points();
    std::cout << "✅ ALL Boolean Plane Tests Passed" << std::endl;
    return 0;
}
