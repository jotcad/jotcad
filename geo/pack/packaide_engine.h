#ifndef PACKAIDE_ENGINE_H
#define PACKAIDE_ENGINE_H

#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "packaide_core.h"
#include "../../fs/cpp/vfs_node.h"
#include "geometry.h"
#include "matrix.h"
#include "shape.h"

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Constrained_triangulation_plus_2.h>
#include <CGAL/Polyline_simplification_2/simplify.h>
#include <CGAL/Polyline_simplification_2/Squared_distance_cost.h>

namespace pack {

using namespace jotcad::geo;

namespace PS = CGAL::Polyline_simplification_2;

class PackaideEngine {
public:
    struct Config {
        packaide::FT spacing = packaide::FT(2.0);
        packaide::FT margin = packaide::FT(5.0);
        packaide::FT simplification_tolerance = packaide::FT(0.1);
    };

    struct PackResult {
        std::vector<Shape> bins;
        std::vector<std::string> unplaced;
    };

    typedef PS::Vertex_base_2<packaide::K> Vb;
    typedef CGAL::Constrained_triangulation_face_base_2<packaide::K> Fb;
    typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
    typedef CGAL::Constrained_Delaunay_triangulation_2<packaide::K, TDS, CGAL::Exact_intersections_tag> CDT;
    typedef CGAL::Constrained_triangulation_plus_2<CDT> CT;

    /**
     * @brief Simplifies a Polygon_with_holes_2 using topology-preserving vertex removal.
     */
    static packaide::Polygon_with_holes_2 simplify_pwh(const packaide::Polygon_with_holes_2& pwh, packaide::FT tolerance) {
        if (tolerance <= packaide::FT(0)) return pwh;
        
        // Standard topology-preserving simplification.
        // This removes points that don't contribute significantly to the shape
        // based on the Squared Distance Cost.
        return PS::simplify(pwh, PS::Squared_distance_cost(), PS::Stop_above_cost_threshold(CGAL::to_double(tolerance)));
    }

    static packaide::Polygon_with_holes_2 geometry_to_cgal(const Geometry& geo) {
        if (geo.faces.empty()) return {};
        const auto& loops = geo.faces[0].loops;
        if (loops.empty()) return {};

        auto get_loop = [&](const std::vector<int>& loop) {
            packaide::Polygon_2 p;
            for (size_t i = 0; i < loop.size(); ++i) {
                packaide::Point_2 pt(geo.vertices[loop[i]].x, geo.vertices[loop[i]].y);
                // Check for sequential duplicates
                if (i > 0) {
                    assert(packaide::Point_2(geo.vertices[loop[i-1]].x, geo.vertices[loop[i-1]].y) != pt);
                }
                p.push_back(pt);
            }
            // Check wrap-around duplicate
            if (p.size() > 1) {
                assert(p.vertex(0) != p.vertex(p.size() - 1));
            }
            return p;
        };

        packaide::Polygon_2 outer = get_loop(loops[0]);
        assert(packaide::is_good_polygon(outer));
        if (outer.is_clockwise_oriented()) outer.reverse_orientation();

        packaide::Polygon_with_holes_2 pwh(outer);
        for (size_t i = 1; i < loops.size(); ++i) {
            packaide::Polygon_2 hole = get_loop(loops[i]);
            assert(packaide::is_good_polygon(hole));
            if (hole.is_counterclockwise_oriented()) hole.reverse_orientation();
            pwh.add_hole(hole);
        }
        return pwh;
    }

    struct PartInfo {
        size_t original_index;
        packaide::Polygon_with_holes_2 cgal_poly;
        packaide::FT area;
        packaide::FT xmin_off, ymin_off;
    };

    struct Placement {
        size_t original_index;
        packaide::FT x, y;
    };

    static std::vector<Placement> pack_geometric(const std::vector<PartInfo>& parts, packaide::Sheet& bin_sheet, const Config& config) {
        std::vector<Placement> placements;
        packaide::BoundingBoxHeuristic heuristic;

        for (const auto& info : parts) {
            std::vector<packaide::Polygon_with_holes_2> islands;
            bin_sheet.material.polygons_with_holes(std::back_inserter(islands));
            
            packaide::Polygon_set_2 feasible_region;
            struct Candidate {
                packaide::Point_2 pos;
                packaide::FT island_area;
            };
            std::vector<Candidate> candidates;

            for (const auto& island : islands) {
                packaide::FT island_area = island.outer_boundary().area();
                auto island_ifp = packaide::compute_ifp_pwh(island, info.cgal_poly);
                
                auto add_candidates = [&](const packaide::Polygon_set_2& set) {
                    if (set.is_empty()) return;
                    std::vector<packaide::Polygon_with_holes_2> pwhs;
                    set.polygons_with_holes(std::back_inserter(pwhs));
                    for (const auto& pwh : pwhs) {
                        for (auto v = pwh.outer_boundary().vertices_begin(); v != pwh.outer_boundary().vertices_end(); ++v) 
                            candidates.push_back({*v, island_area});
                        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                            for (auto v = hit->vertices_begin(); v != hit->vertices_end(); ++v) 
                                candidates.push_back({*v, island_area});
                        }
                    }
                };

                if (!island_ifp.is_empty()) {
                    add_candidates(island_ifp);
                } else {
                    // Exact fit check
                    auto bb_A = island.outer_boundary().bbox();
                    auto bb_B = info.cgal_poly.outer_boundary().bbox();
                    if (packaide::FT(bb_A.xmax() - bb_A.xmin()) >= packaide::FT(bb_B.xmax() - bb_B.xmin()) &&
                        packaide::FT(bb_A.ymax() - bb_A.ymin()) >= packaide::FT(bb_B.ymax() - bb_B.ymin())) {
                        candidates.push_back({packaide::Point_2(bb_A.xmin(), bb_A.ymin()), island_area});
                    }
                }
            }

            if (candidates.empty()) continue;

            packaide::Point_2 best_pos;
            packaide::FT best_score = packaide::FT(1e36); // Area * Area scale
            bool found = false;

            // We use the overall sheet bounds for the Top-Left bias calculation
            std::vector<packaide::Polygon_with_holes_2> all_sheet_islands;
            bin_sheet.material.polygons_with_holes(std::back_inserter(all_sheet_islands));
            packaide::FT sheet_min_x(1e18), sheet_max_y(-1e18);
            for (const auto& i : all_sheet_islands) {
                auto ibb = i.outer_boundary().bbox();
                sheet_min_x = std::min(sheet_min_x, packaide::FT(ibb.xmin()));
                sheet_max_y = std::max(sheet_max_y, packaide::FT(ibb.ymax()));
            }

            for (const auto& cand : candidates) {
                packaide::Transformation tr(CGAL::TRANSLATION, packaide::Vector_2(cand.pos.x(), cand.pos.y()));
                auto candidate_poly = CGAL::transform(tr, info.cgal_poly);
                
                packaide::Polygon_set_2 candidate_set(candidate_poly);
                candidate_set.intersection(bin_sheet.material);
                if (candidate_set.is_empty()) continue;

                // RANKING FORMULA (Lexicographical weights):
                // 1. Bounding Box Area Expansion (Weight: 10^18)
                packaide::FT bb_area = heuristic.score_after_adding(candidate_poly);
                
                // 2. Island Area (Weight: 10^9) - Prefer smallest hole
                packaide::FT island_area = cand.island_area;

                // 3. Top-Left Penalty (Weight: 1) - Prefer Min X, Max Y
                // Penalty = (x - sheet_min_x) + (sheet_max_y - y)
                packaide::FT top_left_penalty = (cand.pos.x() - sheet_min_x) + (sheet_max_y - cand.pos.y());

                packaide::FT current_score = (bb_area * packaide::FT(1e18)) + 
                                           (island_area * packaide::FT(1e9)) + 
                                           top_left_penalty;

                if (current_score < best_score) {
                    best_score = current_score;
                    best_pos = cand.pos;
                    found = true;
                }
            }

            if (found) {
                packaide::Transformation final_tr(CGAL::TRANSLATION, packaide::Vector_2(best_pos.x(), best_pos.y()));
                auto placed_poly = CGAL::transform(final_tr, info.cgal_poly);
                
                // CONSUME MATERIAL
                if (config.spacing > packaide::FT(0)) {
                    auto square = packaide::create_offset_square(config.spacing);
                    auto consumed_poly = CGAL::minkowski_sum_2(placed_poly, square);
                    bin_sheet.material.difference(consumed_poly);
                } else {
                    bin_sheet.material.difference(placed_poly);
                }
                
                heuristic.add(placed_poly);
                placements.push_back({info.original_index, best_pos.x(), best_pos.y()});
            }
        }
        return placements;
    }

    static PackResult pack(fs::VFSNode* vfs, const std::vector<Shape>& parts, const std::vector<Shape>& sheets, const Config& config) {
        PackResult result;
        if (sheets.empty()) return result;

        // 1. Prepare Parts
        std::vector<PartInfo> remaining_parts;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (!parts[i].geometry.has_value()) continue;
            auto geo = vfs->read<Geometry>(parts[i].geometry.value());
            auto cgal_poly = geometry_to_cgal(geo);
            if (cgal_poly.outer_boundary().is_empty()) continue;

            // Simplify part into a conservative outer envelope for performance
            if (config.simplification_tolerance > packaide::FT(0)) {
                cgal_poly = simplify_pwh(cgal_poly, config.simplification_tolerance);
            }

            auto bb = cgal_poly.outer_boundary().bbox();
            packaide::Transformation translate(CGAL::TRANSLATION, packaide::Vector_2(-bb.xmin(), -bb.ymin()));
            cgal_poly = CGAL::transform(translate, cgal_poly);

            remaining_parts.push_back({
                i, 
                cgal_poly, 
                cgal_poly.outer_boundary().area(),
                packaide::FT(bb.xmin()),
                packaide::FT(bb.ymin())
            });
        }

        std::sort(remaining_parts.begin(), remaining_parts.end(), [](const PartInfo& a, const PartInfo& b) {
            return a.area > b.area;
        });

        // 2. Iterate Sheets
        for (size_t s_idx = 0; s_idx < sheets.size(); ++s_idx) {
            if (remaining_parts.empty()) break;
            if (!sheets[s_idx].geometry.has_value()) continue;

            auto sheet_geo = vfs->read<Geometry>(sheets[s_idx].geometry.value());
            auto s_bb = sheet_geo.bounds();
            packaide::FT sw = packaide::FT(s_bb.xmax() - s_bb.xmin()) - packaide::FT(2.0) * config.margin;
            packaide::FT sh = packaide::FT(s_bb.ymax() - s_bb.ymin()) - packaide::FT(2.0) * config.margin;

            if (sw <= packaide::FT(0) || sh <= packaide::FT(0)) continue;

            packaide::Sheet bin_sheet = packaide::Sheet::rectangle(sw, sh);
            
            auto placements = pack_geometric(remaining_parts, bin_sheet, config);

            Shape out_bin = sheets[s_idx];
            out_bin.add_tag("type", "group");
            out_bin.add_tag("name", "sheet_" + std::to_string(s_idx));
            out_bin.components.clear();

            std::set<size_t> placed_indices;
            for (const auto& p : placements) {
                placed_indices.insert(p.original_index);
                
                auto it = std::find_if(remaining_parts.begin(), remaining_parts.end(), [&](const PartInfo& info) {
                    return info.original_index == p.original_index;
                });

                Shape comp = parts[p.original_index];
                Matrix m = Matrix::translate(FT(p.x + packaide::FT(s_bb.xmin()) + config.margin), 
                                           FT(p.y + packaide::FT(s_bb.ymin()) + config.margin), 
                                           FT(0));
                m = m * Matrix::translate(FT(-it->xmin_off), FT(-it->ymin_off), FT(0));
                comp.tf = m;
                out_bin.components.push_back(comp);
            }

            std::vector<PartInfo> next_remaining;
            for (const auto& info : remaining_parts) {
                if (placed_indices.find(info.original_index) == placed_indices.end()) {
                    next_remaining.push_back(info);
                }
            }

            result.bins.push_back(out_bin);
            remaining_parts = next_remaining;
        }

        for (const auto& info : remaining_parts) {
            result.unplaced.push_back(parts[info.original_index].geometry.value().value);
        }

        return result;
    }
};

} // namespace pack

#endif
