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

namespace pack {

using namespace jotcad::geo;

class PackaideEngine {
public:
    struct Config {
        packaide::FT spacing = packaide::FT(2.0);
        packaide::FT margin = packaide::FT(5.0);
    };

    struct PackResult {
        std::vector<Shape> bins;
        std::vector<std::string> unplaced;
    };

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
        std::vector<packaide::Polygon_with_holes_2> placed_polys;
        packaide::BoundingBoxHeuristic heuristic;

        for (const auto& info : parts) {
            auto ifp = packaide::compute_ifp(bin_sheet, info.cgal_poly);
            
            std::vector<packaide::Point_2> candidates;
            bool has_2d_region = false;

            if (!ifp.is_empty()) {
                packaide::Polygon_set_2 feasible_region(ifp);
                for (const auto& placed_poly : placed_polys) {
                    auto nfp = packaide::compute_nfp(placed_poly, info.cgal_poly, config.spacing);
                    feasible_region.difference(nfp);
                }
                if (!feasible_region.is_empty()) {
                    has_2d_region = true;
                    std::vector<packaide::Polygon_with_holes_2> pwhs;
                    feasible_region.polygons_with_holes(std::back_inserter(pwhs));
                    for (const auto& pwh : pwhs) {
                        for (auto v = pwh.outer_boundary().vertices_begin(); v != pwh.outer_boundary().vertices_end(); ++v) candidates.push_back(*v);
                        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                            for (auto v = hit->vertices_begin(); v != hit->vertices_end(); ++v) candidates.push_back(*v);
                        }
                    }
                }
            } else {
                // Potential exact fit check
                auto bb = info.cgal_poly.outer_boundary().bbox();
                if (packaide::FT(bb.xmax() - bb.xmin()) <= bin_sheet.width && 
                    packaide::FT(bb.ymax() - bb.ymin()) <= bin_sheet.height) {
                    candidates.push_back(packaide::Point_2(0, 0));
                }
            }

            if (candidates.empty()) continue;

            packaide::Point_2 best_pos;
            packaide::FT best_score = packaide::FT(1e18); 
            bool found = false;

            for (const auto& v : candidates) {
                if (!has_2d_region) {
                    bool collision = false;
                    packaide::Transformation tr(CGAL::TRANSLATION, packaide::Vector_2(v.x(), v.y()));
                    auto candidate_poly = CGAL::transform(tr, info.cgal_poly);
                    for (const auto& placed : placed_polys) {
                        if (CGAL::do_intersect(candidate_poly, placed)) {
                            collision = true;
                            break;
                        }
                    }
                    if (collision) continue;
                }

                packaide::Transformation tr(CGAL::TRANSLATION, packaide::Vector_2(v.x(), v.y()));
                auto candidate_poly = CGAL::transform(tr, info.cgal_poly);
                packaide::FT score = heuristic.score_after_adding(candidate_poly);
                if (score < best_score) {
                    best_score = score;
                    best_pos = v;
                    found = true;
                }
            }

            if (found) {
                packaide::Transformation final_tr(CGAL::TRANSLATION, packaide::Vector_2(best_pos.x(), best_pos.y()));
                auto placed_poly = CGAL::transform(final_tr, info.cgal_poly);
                placed_polys.push_back(placed_poly);
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

            packaide::Sheet bin_sheet = { sw, sh };
            
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
