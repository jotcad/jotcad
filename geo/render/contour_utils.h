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
        static const std::map<std::string, ColorRGB> named_colors = {
            {"aliceblue", {240, 248, 255}},
            {"antiquewhite", {250, 235, 215}},
            {"aqua", {0, 255, 255}},
            {"aquamarine", {127, 255, 212}},
            {"azure", {240, 255, 255}},
            {"beige", {245, 245, 220}},
            {"bisque", {255, 228, 196}},
            {"black", {0, 0, 0}},
            {"blanchedalmond", {255, 235, 205}},
            {"blue", {0, 0, 255}},
            {"blueviolet", {138, 43, 226}},
            {"brown", {165, 42, 42}},
            {"burlywood", {222, 184, 135}},
            {"cadetblue", {95, 158, 160}},
            {"chartreuse", {127, 255, 0}},
            {"chocolate", {210, 105, 30}},
            {"coral", {255, 127, 80}},
            {"cornflowerblue", {100, 149, 237}},
            {"cornsilk", {255, 248, 220}},
            {"crimson", {220, 20, 60}},
            {"cyan", {0, 255, 255}},
            {"darkblue", {0, 0, 139}},
            {"darkcyan", {0, 139, 139}},
            {"darkgoldenrod", {184, 134, 11}},
            {"darkgray", {169, 169, 169}},
            {"darkgreen", {0, 100, 0}},
            {"darkgrey", {169, 169, 169}},
            {"darkkhaki", {189, 183, 107}},
            {"darkmagenta", {139, 0, 139}},
            {"darkolivegreen", {85, 107, 47}},
            {"darkorange", {255, 140, 0}},
            {"darkorchid", {153, 50, 204}},
            {"darkred", {139, 0, 0}},
            {"darksalmon", {233, 150, 122}},
            {"darkseagreen", {143, 188, 143}},
            {"darkslateblue", {72, 61, 139}},
            {"darkslategrey", {47, 79, 79}},
            {"darkturquoise", {0, 206, 209}},
            {"darkviolet", {148, 0, 211}},
            {"deeppink", {255, 20, 147}},
            {"deepskyblue", {0, 191, 255}},
            {"dimgray", {105, 105, 105}},
            {"dimgrey", {105, 105, 105}},
            {"dodgerblue", {30, 144, 255}},
            {"firebrick", {178, 34, 34}},
            {"floralwhite", {255, 250, 240}},
            {"forestgreen", {34, 139, 34}},
            {"fuchsia", {255, 0, 255}},
            {"gainsboro", {220, 220, 220}},
            {"ghostwhite", {248, 248, 255}},
            {"gold", {255, 215, 0}},
            {"goldenrod", {218, 165, 32}},
            {"gray", {128, 128, 128}},
            {"green", {0, 128, 0}},
            {"greenyellow", {173, 255, 47}},
            {"grey", {128, 128, 128}},
            {"honeydew", {240, 255, 240}},
            {"hotpink", {255, 105, 180}},
            {"indianred", {205, 92, 92}},
            {"indigo", {75, 0, 130}},
            {"ivory", {255, 255, 240}},
            {"khaki", {240, 230, 140}},
            {"lavender", {230, 230, 250}},
            {"lavenderblush", {255, 240, 245}},
            {"lawngreen", {124, 252, 0}},
            {"lemonchiffon", {255, 250, 205}},
            {"lightblue", {173, 216, 230}},
            {"lightcoral", {240, 128, 128}},
            {"lightcyan", {224, 255, 255}},
            {"lightgoldenrodyellow", {250, 250, 210}},
            {"lightgray", {211, 211, 211}},
            {"lightgreen", {144, 238, 144}},
            {"lightgrey", {211, 211, 211}},
            {"lightpink", {255, 182, 193}},
            {"lightsalmon", {255, 160, 122}},
            {"lightseagreen", {32, 178, 170}},
            {"lightskyblue", {135, 206, 250}},
            {"lightslategray", {119, 136, 153}},
            {"lightslategrey", {119, 136, 153}},
            {"lightsteelblue", {176, 196, 222}},
            {"lightyellow", {255, 255, 224}},
            {"lime", {0, 255, 0}},
            {"limegreen", {50, 205, 50}},
            {"linen", {250, 240, 230}},
            {"magenta", {255, 0, 255}},
            {"maroon", {128, 0, 0}},
            {"mediumaquamarine", {102, 205, 170}},
            {"mediumblue", {0, 0, 205}},
            {"mediumorchid", {186, 85, 211}},
            {"mediumpurple", {147, 112, 219}},
            {"mediumseagreen", {60, 179, 113}},
            {"mediumslateblue", {123, 104, 238}},
            {"mediumspringgreen", {0, 250, 154}},
            {"mediumturquoise", {72, 209, 204}},
            {"mediumvioletred", {199, 21, 133}},
            {"midnightblue", {25, 25, 112}},
            {"mintcream", {245, 255, 250}},
            {"mistyrose", {255, 228, 225}},
            {"moccasin", {255, 228, 181}},
            {"navajowhite", {255, 222, 173}},
            {"navy", {0, 0, 128}},
            {"oldlace", {253, 245, 230}},
            {"olive", {128, 128, 0}},
            {"olivedrab", {107, 142, 35}},
            {"orange", {255, 165, 0}},
            {"orangered", {255, 69, 0}},
            {"orchid", {218, 112, 214}},
            {"palegoldenrod", {238, 232, 170}},
            {"palegreen", {152, 251, 152}},
            {"paleturquoise", {175, 238, 238}},
            {"palevioletred", {219, 112, 147}},
            {"papayawhip", {255, 239, 213}},
            {"peachpuff", {255, 218, 185}},
            {"peru", {205, 133, 63}},
            {"pink", {255, 192, 203}},
            {"plum", {221, 160, 221}},
            {"powderblue", {176, 224, 230}},
            {"purple", {128, 0, 128}},
            {"rebeccapurple", {102, 51, 153}},
            {"red", {255, 0, 0}},
            {"rosybrown", {188, 143, 143}},
            {"royalblue", {65, 105, 225}},
            {"saddlebrown", {139, 69, 19}},
            {"salmon", {250, 128, 114}},
            {"sandybrown", {244, 164, 96}},
            {"seagreen", {46, 139, 87}},
            {"seashell", {255, 245, 238}},
            {"sienna", {160, 82, 45}},
            {"silver", {192, 192, 192}},
            {"skyblue", {135, 206, 235}},
            {"slateblue", {106, 90, 205}},
            {"slategray", {112, 128, 144}},
            {"slategrey", {112, 128, 144}},
            {"snow", {255, 250, 250}},
            {"springgreen", {0, 255, 127}},
            {"steelblue", {70, 130, 180}},
            {"tan", {210, 180, 140}},
            {"teal", {0, 128, 128}},
            {"thistle", {216, 191, 216}},
            {"tomato", {255, 99, 71}},
            {"turquoise", {64, 224, 208}},
            {"violet", {238, 130, 238}},
            {"wheat", {245, 222, 179}},
            {"white", {255, 255, 255}},
            {"whitesmoke", {245, 245, 245}},
            {"yellow", {255, 255, 0}},
            {"yellowgreen", {154, 205, 50}}
        };
        auto it = named_colors.find(hex);
        if (it != named_colors.end()) return it->second;
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

    static std::vector<std::vector<std::pair<EK::Point_2, EK::Point_2>>> generate_marching_triangles_segments(
        const std::vector<int>& padded, int p_w, int p_h, int H, const std::vector<int>& targets) {
        // Build active target lookup
        std::map<int, int> label_to_idx;
        for (size_t i = 0; i < targets.size(); ++i) {
            label_to_idx[targets[i]] = i;
        }

        std::vector<std::vector<std::pair<EK::Point_2, EK::Point_2>>> result(targets.size());

        for (int y = 0; y < p_h - 1; ++y) {
            for (int x = 0; x < p_w - 1; ++x) {
                int v0_lbl = padded[y*p_w+x];
                int v1_lbl = padded[y*p_w+(x+1)];
                int v2_lbl = padded[(y+1)*p_w+(x+1)];
                int v3_lbl = padded[(y+1)*p_w+x];

                if (v0_lbl == v1_lbl && v1_lbl == v2_lbl && v2_lbl == v3_lbl) continue;

                int active_colors[4];
                int active_count = 0;
                auto add_target = [&](int lbl) {
                    auto it = label_to_idx.find(lbl);
                    if (it == label_to_idx.end()) return;
                    for (int i = 0; i < active_count; ++i) {
                        if (active_colors[i] == lbl) return;
                    }
                    active_colors[active_count++] = lbl;
                };
                add_target(v0_lbl);
                add_target(v1_lbl);
                add_target(v2_lbl);
                add_target(v3_lbl);

                if (active_count == 0) continue;

                FT fx(x-1), fy(H - (y-1)), h = FT(1)/2;
                EK::Point_2 e0(fx+h, fy);
                EK::Point_2 e1(fx+1, fy-h);
                EK::Point_2 e2(fx+h, fy-1);
                EK::Point_2 e3(fx, fy-h);
                EK::Point_2 d0(fx+h, fy-h);

                for (int idx = 0; idx < active_count; ++idx) {
                    int c = active_colors[idx];
                    int target_idx = label_to_idx[c];
                    auto& segments = result[target_idx];

                    // Triangle 1: v0_lbl, v1_lbl, v3_lbl
                    bool v0 = (v0_lbl == c);
                    bool v1 = (v1_lbl == c);
                    bool v3 = (v3_lbl == c);
                    if (v0) {
                        if (!v1 && !v3) {
                            if (v1_lbl == v3_lbl) segments.push_back({e0, e3});
                            else { segments.push_back({e0, d0}); segments.push_back({d0, e3}); }
                        } else if (v1 && !v3) segments.push_back({d0, e3});
                        else if (v3 && !v1) segments.push_back({e0, d0});
                    } else {
                        if (v1 && v3) segments.push_back({e0, e3});
                        else if (v1 && !v3) segments.push_back({e0, d0});
                        else if (v3 && !v1) segments.push_back({d0, e3});
                    }

                    // Triangle 2: v1_lbl, v2_lbl, v3_lbl
                    bool tv1 = (v1_lbl == c);
                    bool tv2 = (v2_lbl == c);
                    bool tv3 = (v3_lbl == c);
                    if (tv1) {
                        if (!tv2 && !tv3) {
                            if (v2_lbl == v3_lbl) segments.push_back({e1, d0});
                            else { segments.push_back({e1, e2}); segments.push_back({e2, d0}); }
                        } else if (tv2 && !tv3) segments.push_back({e2, d0});
                        else if (tv3 && !tv2) segments.push_back({e1, e2});
                    } else {
                        if (tv2 && tv3) segments.push_back({e1, d0});
                        else if (tv2 && !tv3) segments.push_back({e1, e2});
                        else if (tv3 && !tv2) segments.push_back({e2, d0});
                    }
                }
            }
        }
        return result;
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
