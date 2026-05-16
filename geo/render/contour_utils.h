#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <list>
#include <chrono>
#include <iostream>
#include <CGAL/Polygon_2.h>
#include "geometry.h"

namespace jotcad {
namespace geo {

typedef CGAL::Polygon_2<EK> Polygon;

struct FaceGroup {
    size_t outer;
    std::vector<size_t> holes;
};

class ContourUtils {
public:
    static std::vector<FaceGroup> group_polygons(const std::vector<Polygon>& polygons) {
        if (polygons.empty()) return {};
        auto t_start = std::chrono::steady_clock::now();
        
        std::vector<bool> is_hole(polygons.size(), false);
        std::vector<CGAL::Bbox_2> bboxes;
        for (const auto& p : polygons) bboxes.push_back(p.bbox());

        for (size_t i = 0; i < polygons.size(); ++i) {
            int parent_count = 0;
            auto test_p = polygons[i][0];
            for (size_t j = 0; j < polygons.size(); ++j) {
                if (i == j) continue;
                // BBox Pruning: skip expensive check if point is outside the bounding box
                if (test_p.x() < bboxes[j].xmin() || test_p.x() > bboxes[j].xmax() ||
                    test_p.y() < bboxes[j].ymin() || test_p.y() > bboxes[j].ymax()) continue;

                if (polygons[j].bounded_side(test_p) == CGAL::ON_BOUNDED_SIDE) parent_count++;
            }
            if (parent_count % 2 != 0) is_hole[i] = true;
            if (i > 0 && i % 1000 == 0) std::cout << "      - group progress: " << (i * 100 / polygons.size()) << "%" << std::endl;
        }

        std::vector<FaceGroup> groups;
        for (size_t i = 0; i < polygons.size(); ++i) {
            if (!is_hole[i]) groups.push_back({i, {}});
        }

        for (size_t i = 0; i < polygons.size(); ++i) {
            if (is_hole[i]) {
                int best_parent = -1;
                auto test_p = polygons[i][0];
                for (size_t g = 0; g < groups.size(); ++g) {
                    size_t outer_idx = groups[g].outer;
                    if (test_p.x() < bboxes[outer_idx].xmin() || test_p.x() > bboxes[outer_idx].xmax() ||
                        test_p.y() < bboxes[outer_idx].ymin() || test_p.y() > bboxes[outer_idx].ymax()) continue;

                    if (polygons[outer_idx].bounded_side(test_p) == CGAL::ON_BOUNDED_SIDE) {
                        if (best_parent == -1 || polygons[groups[best_parent].outer].bounded_side(polygons[groups[g].outer][0]) == CGAL::ON_BOUNDED_SIDE) {
                            best_parent = (int)g;
                        }
                    }
                }
                if (best_parent != -1) groups[best_parent].holes.push_back(i);
            }
        }
        auto t_end = std::chrono::steady_clock::now();
        std::cout << "    [Group] Polys=" << polygons.size() << ", Time=" << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() << "ms" << std::endl;
        return groups;
    }

    struct ColorRGB { uint8_t r, g, b; };

    static ColorRGB parse_color(const std::string& hex) {
        if (hex.empty()) return {0,0,0};
        if (hex == "red") return {255, 0, 0};
        if (hex == "green") return {0, 255, 0};
        if (hex == "blue") return {0, 0, 255};
        if (hex == "black") return {0, 0, 0};
        if (hex == "white") return {255, 255, 255};
        std::string s = (hex[0] == '#') ? hex.substr(1) : hex;
        try {
            if (s.size() == 3) {
                int r = std::stoi(s.substr(0,1), nullptr, 16), g = std::stoi(s.substr(1,1), nullptr, 16), b = std::stoi(s.substr(2,1), nullptr, 16);
                return {(uint8_t)(r*17), (uint8_t)(g*17), (uint8_t)(b*17)};
            }
            if (s.size() >= 6) {
                int r = std::stoi(s.substr(0,2), nullptr, 16), g = std::stoi(s.substr(2,2), nullptr, 16), b = std::stoi(s.substr(4,2), nullptr, 16);
                return {(uint8_t)r, (uint8_t)g, (uint8_t)b};
            }
        } catch (...) {}
        return {0,0,0};
    }
    
    struct PointKey { 
        long long x, y; 
        bool operator<(const PointKey& o) const { if (x != o.x) return x < o.x; return y < o.y; }
    };

    static std::vector<Polygon> weld_segments(const std::vector<std::pair<EK::Point_2, EK::Point_2>>& segments, double tolerance = 0.5, double min_area = 16.0) {
        if (segments.empty()) return {};
        auto t_start = std::chrono::steady_clock::now();

        std::map<PointKey, std::vector<EK::Point_2>> adj;
        auto to_key = [](const EK::Point_2& v) { 
            return PointKey{ (long long)std::round(CGAL::to_double(v.x()) * 1000000), 
                             (long long)std::round(CGAL::to_double(v.y()) * 1000000) }; 
        };
        for (const auto& seg : segments) {
            adj[to_key(seg.first)].push_back(seg.second);
            adj[to_key(seg.second)].push_back(seg.first);
        }

        auto t_adj = std::chrono::steady_clock::now();
        std::vector<Polygon> polygons;
        std::map<PointKey, bool> visited;

        for (auto const& [key, neighbors] : adj) {
            if (visited[key] || neighbors.empty()) continue;

            std::vector<EK::Point_2> loop;
            PointKey curr_key = key;
            loop.push_back(EK::Point_2(FT(key.x) / 1000000, FT(key.y) / 1000000));
            visited[curr_key] = true;

            while (true) {
                bool found = false;
                for (const auto& next_v : adj[curr_key]) {
                    PointKey next_key = to_key(next_v);
                    if (!visited[next_key]) {
                        visited[next_key] = true;
                        curr_key = next_key;
                        loop.push_back(next_v);
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }

            if (loop.size() >= 3) {
                std::vector<EK::Point_2> simplified = simplify_douglas_peucker(loop, tolerance);
                if (simplified.size() >= 3) {
                    Polygon poly;
                    for (const auto& p : simplified) poly.push_back(p);
                    // Filter by Area to remove speckle noise
                    if (poly.is_simple() && std::abs(CGAL::to_double(poly.area())) > min_area) polygons.push_back(poly);
                }
            }
        }
        auto t_end = std::chrono::steady_clock::now();
        std::cout << "    [Weld] Segs=" << segments.size() << ", Polys=" << polygons.size() << ", Total=" << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() << "ms" << std::endl;
        
        return polygons;
    }

private:
    static std::vector<EK::Point_2> simplify_douglas_peucker(const std::vector<EK::Point_2>& pts, double tolerance) {
        if (pts.size() < 3 || tolerance <= 1e-9) return pts;
        std::vector<bool> keep(pts.size(), false);
        keep[0] = true;
        keep[pts.size()-1] = true;
        simplify_recursive(pts, 0, (int)pts.size()-1, tolerance * tolerance, keep);
        std::vector<EK::Point_2> result;
        for (size_t i = 0; i < pts.size(); ++i) if (keep[i]) result.push_back(pts[i]);
        return result;
    }

    static void simplify_recursive(const std::vector<EK::Point_2>& pts, int start, int end, double tolSq, std::vector<bool>& keep) {
        if (end <= start + 1) return;
        double maxDistSq = 0;
        int index = start;
        EK::Line_2 line(pts[start], pts[end]);
        for (int i = start + 1; i < end; ++i) {
            double dSq = CGAL::to_double(CGAL::squared_distance(line, pts[i]));
            if (dSq > maxDistSq) { maxDistSq = dSq; index = i; }
        }
        if (maxDistSq > tolSq) {
            keep[index] = true;
            simplify_recursive(pts, start, index, tolSq, keep);
            simplify_recursive(pts, index, end, tolSq, keep);
        }
    }
};

} // namespace geo
} // namespace jotcad
