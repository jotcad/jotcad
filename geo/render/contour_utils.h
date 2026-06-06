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
        
        auto get_inside_point = [](const Polygon& poly) {
            if (poly.size() < 3) return poly[0];
            size_t min_idx = 0;
            auto min_x = poly[0].x();
            for (size_t v = 1; v < poly.size(); ++v) {
                if (poly[v].x() < min_x) {
                    min_x = poly[v].x();
                    min_idx = v;
                }
            }
            size_t prev_idx = (min_idx + poly.size() - 1) % poly.size();
            size_t next_idx = (min_idx + 1) % poly.size();
            return EK::Point_2(
                (poly[min_idx].x() * 2 + poly[prev_idx].x() + poly[next_idx].x()) / 4,
                (poly[min_idx].y() * 2 + poly[prev_idx].y() + poly[next_idx].y()) / 4
            );
        };

        std::vector<bool> is_hole(polygons.size(), false);
        std::vector<CGAL::Bbox_2> bboxes;
        for (const auto& p : polygons) bboxes.push_back(p.bbox());

        for (size_t i = 0; i < polygons.size(); ++i) {
            int parent_count = 0;
            auto test_p = get_inside_point(polygons[i]);
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
                auto test_p = get_inside_point(polygons[i]);
                for (size_t g = 0; g < groups.size(); ++g) {
                    size_t outer_idx = groups[g].outer;
                    if (test_p.x() < bboxes[outer_idx].xmin() || test_p.x() > bboxes[outer_idx].xmax() ||
                        test_p.y() < bboxes[outer_idx].ymin() || test_p.y() > bboxes[outer_idx].ymax()) continue;

                    if (polygons[outer_idx].bounded_side(test_p) == CGAL::ON_BOUNDED_SIDE) {
                        if (best_parent == -1 || polygons[groups[best_parent].outer].bounded_side(get_inside_point(polygons[groups[g].outer])) == CGAL::ON_BOUNDED_SIDE) {
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
        bool operator==(const PointKey& o) const { return x == o.x && y == o.y; }
        bool operator!=(const PointKey& o) const { return !(*this == o); }
    };

    static std::vector<Polygon> weld_segments(const std::vector<std::pair<EK::Point_2, EK::Point_2>>& segments, double tolerance = 0.5, double min_area = 16.0, bool prune_collinear = true) {
        if (segments.empty()) return {};
        auto t_start = std::chrono::steady_clock::now();

        auto to_key = [](const EK::Point_2& v) { 
            return PointKey{ (long long)std::round(CGAL::to_double(v.x()) * 1000000), 
                             (long long)std::round(CGAL::to_double(v.y()) * 1000000) }; 
        };

        struct TrackedSegment {
            EK::Point_2 p1;
            EK::Point_2 p2;
            bool used = false;
        };

        std::vector<TrackedSegment> track_segs;
        track_segs.reserve(segments.size());
        for (const auto& seg : segments) {
            track_segs.push_back({seg.first, seg.second, false});
        }

        std::map<PointKey, std::vector<size_t>> adj;
        for (size_t i = 0; i < track_segs.size(); ++i) {
            adj[to_key(track_segs[i].p1)].push_back(i);
            adj[to_key(track_segs[i].p2)].push_back(i);
        }

        auto t_adj = std::chrono::steady_clock::now();
        std::vector<Polygon> polygons;

        for (size_t i = 0; i < track_segs.size(); ++i) {
            if (track_segs[i].used) continue;

            std::vector<EK::Point_2> loop;
            std::map<PointKey, size_t> loop_indices;

            track_segs[i].used = true;
            EK::Point_2 start_pt = track_segs[i].p1;
            EK::Point_2 curr_pt = track_segs[i].p2;
            
            loop.push_back(start_pt);
            loop_indices[to_key(start_pt)] = 0;

            loop.push_back(curr_pt);
            loop_indices[to_key(curr_pt)] = 1;

            PointKey start_key = to_key(start_pt);

            while (true) {
                PointKey curr_key = to_key(curr_pt);
                if (curr_key == start_key) {
                    break;
                }

                bool found = false;
                const auto& neighbors = adj[curr_key];
                for (size_t seg_idx : neighbors) {
                    if (!track_segs[seg_idx].used) {
                        track_segs[seg_idx].used = true;
                        EK::Point_2 next_pt = (to_key(track_segs[seg_idx].p1) == curr_key) ? track_segs[seg_idx].p2 : track_segs[seg_idx].p1;
                        PointKey next_key = to_key(next_pt);
                        
                        if (next_key != start_key) {
                            auto it = loop_indices.find(next_key);
                            if (it != loop_indices.end()) {
                                size_t cycle_start_idx = it->second;
                                std::vector<EK::Point_2> sub_cycle;
                                sub_cycle.reserve(loop.size() - cycle_start_idx);
                                for (size_t k = cycle_start_idx; k < loop.size(); ++k) {
                                    sub_cycle.push_back(loop[k]);
                                    loop_indices.erase(to_key(loop[k]));
                                }
                                
                                if (sub_cycle.size() >= 3) {
                                    std::vector<EK::Point_2> simplified = simplify_douglas_peucker(sub_cycle, tolerance);
                                    if (prune_collinear) {
                                        simplified = remove_collinear(simplified);
                                    }
                                    if (simplified.size() >= 3) {
                                        Polygon poly;
                                        for (const auto& p : simplified) poly.push_back(p);
                                        if (poly.is_simple() && std::abs(CGAL::to_double(poly.area())) > min_area) {
                                            polygons.push_back(poly);
                                        }
                                    }
                                }
                                loop.resize(cycle_start_idx);
                            }
                        }

                        curr_pt = next_pt;
                        loop.push_back(curr_pt);
                        loop_indices[to_key(curr_pt)] = loop.size() - 1;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }

            if (loop.size() >= 3) {
                if (to_key(loop.back()) == start_key) {
                    loop.pop_back();
                }
                if (loop.size() >= 3) {
                    std::vector<EK::Point_2> simplified = simplify_douglas_peucker(loop, tolerance);
                    if (prune_collinear) {
                        simplified = remove_collinear(simplified);
                    }
                    if (simplified.size() >= 3) {
                        Polygon poly;
                        for (const auto& p : simplified) poly.push_back(p);
                        if (poly.is_simple() && std::abs(CGAL::to_double(poly.area())) > min_area) {
                            polygons.push_back(poly);
                        }
                    }
                }
            }
        }

        auto t_end = std::chrono::steady_clock::now();
        std::cout << "    [Weld] Segs=" << segments.size() << ", Polys=" << polygons.size() << ", Total=" << std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count() << "ms" << std::endl;
        
        return polygons;
    }

private:
    static std::vector<EK::Point_2> remove_collinear(const std::vector<EK::Point_2>& pts) {
        if (pts.size() < 3) return pts;
        std::vector<EK::Point_2> result;
        result.reserve(pts.size());
        for (size_t i = 0; i < pts.size(); ++i) {
            const auto& p = pts[(i == 0) ? pts.size() - 1 : i - 1];
            const auto& q = pts[i];
            const auto& r = pts[(i == pts.size() - 1) ? 0 : i + 1];
            if (!CGAL::collinear(p, q, r)) {
                result.push_back(q);
            }
        }
        if (result.size() < pts.size() && result.size() >= 3) {
            std::vector<EK::Point_2> final_result;
            final_result.reserve(result.size());
            for (size_t i = 0; i < result.size(); ++i) {
                const auto& p = result[(i == 0) ? result.size() - 1 : i - 1];
                const auto& q = result[i];
                const auto& r = result[(i == result.size() - 1) ? 0 : i + 1];
                if (!CGAL::collinear(p, q, r)) {
                    final_result.push_back(q);
                }
            }
            return final_result;
        }
        return result;
    }

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
