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
        packaide::FT margin = packaide::FT(0.0);
        packaide::FT simplification_tolerance = packaide::FT(0.1);
        int rotations = 1; 
    };

    struct PackResult {
        std::vector<Shape> bins;
        std::vector<Geometry> remainders;
        std::vector<std::string> unplaced;
    };

    typedef PS::Vertex_base_2<packaide::K> Vb;
    typedef CGAL::Constrained_triangulation_face_base_2<packaide::K> Fb;
    typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
    typedef CGAL::Constrained_Delaunay_triangulation_2<packaide::K, TDS, CGAL::Exact_intersections_tag> CDT;
    typedef CGAL::Constrained_triangulation_plus_2<CDT> CT;

    static packaide::Polygon_with_holes_2 simplify_pwh(const packaide::Polygon_with_holes_2& pwh, packaide::FT tolerance) {
        if (tolerance <= packaide::FT(0)) return pwh;
        return PS::simplify(pwh, PS::Squared_distance_cost(), PS::Stop_above_cost_threshold(CGAL::to_double(tolerance)));
    }

    static packaide::Polygon_with_holes_2 geometry_to_cgal(const Geometry& geo, const Matrix& tf = Matrix::identity()) {
        if (geo.faces.empty()) return {};
        const auto& loops = geo.faces[0].loops;
        if (loops.empty()) return {};

        auto get_loop = [&](const std::vector<int>& loop) {
            packaide::Polygon_2 p;
            for (size_t i = 0; i < loop.size(); ++i) {
                Vertex v = geo.vertices[loop[i]];
                Point_3 p3 = tf.transform(Point_3(v.x, v.y, v.z));
                packaide::Point_2 pt(p3.x(), p3.y());
                if (i > 0) {
                    Vertex prev_v = geo.vertices[loop[i-1]];
                    Point_3 prev3 = tf.transform(Point_3(prev_v.x, prev_v.y, prev_v.z));
                    if (packaide::Point_2(prev3.x(), prev3.y()) == pt) continue;
                }
                p.push_back(pt);
            }
            if (p.size() > 1 && p.vertex(0) == p.vertex(p.size() - 1)) {
                p.erase(p.vertices_end() - 1);
            }
            return p;
        };

        packaide::Polygon_2 outer = get_loop(loops[0]);
        if (!packaide::is_good_polygon(outer)) return {};
        if (outer.is_clockwise_oriented()) outer.reverse_orientation();

        packaide::Polygon_with_holes_2 pwh(outer);
        for (size_t i = 1; i < loops.size(); ++i) {
            packaide::Polygon_2 hole = get_loop(loops[i]);
            if (!packaide::is_good_polygon(hole)) continue;
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
        double angle;
    };

    static std::vector<Placement> pack_geometric(const std::vector<PartInfo>& parts, packaide::Sheet& bin_sheet, const Config& config) {
        std::vector<Placement> placements;
        packaide::BoundingBoxHeuristic heuristic;

        for (const auto& info : parts) {
            struct BestCand {
                packaide::Point_2 pos;
                double angle;
                packaide::FT score = packaide::FT(1e36);
                packaide::Polygon_with_holes_2 oriented_poly;
                bool found = false;
            } best;

            std::cout << "[Packaide] Processing part " << info.original_index << " (Area: " << info.area << ")" << std::endl;

            for (int r = 0; r < config.rotations; ++r) {
                double angle_deg = (360.0 / config.rotations) * r;
                double angle_rad = angle_deg * M_PI / 180.0;
                
                packaide::Transformation rotate(CGAL::ROTATION, std::sin(angle_rad), std::cos(angle_rad));
                auto oriented_poly = CGAL::transform(rotate, info.cgal_poly);
                
                auto bb = oriented_poly.outer_boundary().bbox();
                packaide::Transformation normalize(CGAL::TRANSLATION, packaide::Vector_2(-bb.xmin(), -bb.ymin()));
                oriented_poly = CGAL::transform(normalize, oriented_poly);

                std::vector<packaide::Polygon_with_holes_2> islands;
                bin_sheet.material.polygons_with_holes(std::back_inserter(islands));

                for (size_t island_idx = 0; island_idx < islands.size(); ++island_idx) {
                    const auto& island = islands[island_idx];
                    
                    // MANDATE: trust the IFP. It now accounts for the ACTUAL sheet geometry.
                    auto island_ifp = packaide::compute_ifp_pwh(island, oriented_poly);
                    if (island_ifp.is_empty()) continue;

                    std::vector<packaide::Polygon_with_holes_2> pwhs;
                    island_ifp.polygons_with_holes(std::back_inserter(pwhs));
                    
                    for (const auto& pwh : pwhs) {
                        auto process_vertex = [&](const packaide::Point_2& v) {
                            packaide::Transformation tr(CGAL::TRANSLATION, packaide::Vector_2(v.x(), v.y()));
                            auto cand_poly = CGAL::transform(tr, oriented_poly);
                            
                            // Score based on bounding box packing heuristic
                            packaide::FT bb_area = heuristic.score_after_adding(cand_poly);
                            packaide::FT current_score = (bb_area * packaide::FT(1e6)) + v.x() - v.y();

                            if (current_score < best.score) {
                                best.score = current_score;
                                best.pos = v;
                                best.angle = angle_deg;
                                best.oriented_poly = oriented_poly;
                                best.found = true;
                            }
                        };

                        for (auto v = pwh.outer_boundary().vertices_begin(); v != pwh.outer_boundary().vertices_end(); ++v) process_vertex(*v);
                        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                            for (auto v = hit->vertices_begin(); v != hit->vertices_end(); ++v) process_vertex(*v);
                        }
                    }
                }
            }

            if (best.found) {
                std::cout << "  [Packaide] SUCCESS: Placed part " << info.original_index << " at (" << best.pos.x() << ", " << best.pos.y() << ") angle " << best.angle << std::endl;
                packaide::Transformation final_tr(CGAL::TRANSLATION, packaide::Vector_2(best.pos.x(), best.pos.y()));
                auto placed_poly = CGAL::transform(final_tr, best.oriented_poly);
                
                if (config.spacing > packaide::FT(0)) {
                    auto square = packaide::create_offset_square(config.spacing);
                    auto consumed_poly = CGAL::minkowski_sum_2(placed_poly, square);
                    bin_sheet.material.difference(consumed_poly);
                } else {
                    bin_sheet.material.difference(placed_poly);
                }
                
                heuristic.add(placed_poly);
                placements.push_back({info.original_index, best.pos.x(), best.pos.y(), best.angle});
            } else {
                std::cout << "  [Packaide] FAILURE: No valid placement found for part " << info.original_index << std::endl;
            }
        }
        return placements;
    }

    static PackResult pack(fs::VFSNode* vfs, const std::vector<Shape>& parts, const std::vector<Shape>& sheets, const Config& config) {
        PackResult result;
        if (sheets.empty()) return result;

        std::vector<PartInfo> remaining_parts;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (!parts[i].geometry.has_value()) continue;
            auto geo = vfs->read<Geometry>(parts[i].geometry.value());
            auto cgal_poly = geometry_to_cgal(geo, parts[i].tf);
            if (cgal_poly.outer_boundary().is_empty()) continue;

            if (config.simplification_tolerance > packaide::FT(0)) {
                cgal_poly = simplify_pwh(cgal_poly, config.simplification_tolerance);
            }

            auto bb = cgal_poly.outer_boundary().bbox();
            packaide::Transformation normalize(CGAL::TRANSLATION, packaide::Vector_2(-bb.xmin(), -bb.ymin()));
            cgal_poly = CGAL::transform(normalize, cgal_poly);

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

        for (size_t s_idx = 0; s_idx < sheets.size(); ++s_idx) {
            if (remaining_parts.empty()) break;
            if (!sheets[s_idx].geometry.has_value()) continue;

            auto sheet_geo = vfs->read<Geometry>(sheets[s_idx].geometry.value());
            packaide::Sheet bin_sheet;
            bin_sheet.material.insert(geometry_to_cgal(sheet_geo, sheets[s_idx].tf));

            auto placements = pack_geometric(remaining_parts, bin_sheet, config);

            Shape out_bin;
            out_bin.add_tag("type", "group");
            out_bin.add_tag("sheet", (double)(s_idx + 1));
            out_bin.tf = Matrix::identity(); 

            packaide::Polygon_set_2 sheet_remainder;
            sheet_remainder.insert(geometry_to_cgal(sheet_geo, sheets[s_idx].tf));

            std::set<size_t> placed_indices;
            for (const auto& p : placements) {
                placed_indices.insert(p.original_index);
                
                auto it = std::find_if(remaining_parts.begin(), remaining_parts.end(), [&](const PartInfo& info) {
                    return info.original_index == p.original_index;
                });

                Shape comp = parts[p.original_index];
                double turns = p.angle / 360.0;
                double rad = p.angle * M_PI / 180.0;
                Matrix rot_tf = Matrix::rotationZ(turns);
                
                Geometry oriented_geo = vfs->read<Geometry>(comp.geometry.value());
                oriented_geo.apply_tf(rot_tf * comp.tf);
                auto oriented_bb = oriented_geo.bounds();
                
                packaide::FT world_x = p.x + config.margin;
                packaide::FT world_y = p.y + config.margin;
                
                Matrix placement_tf = Matrix::translate(FT(world_x - FT(oriented_bb.xmin())), FT(world_y - FT(oriented_bb.ymin())), FT(0));
                
                comp.tf = placement_tf * rot_tf * comp.tf;
                out_bin.components.push_back(comp);

                packaide::Transformation rotate_cgal(CGAL::ROTATION, std::sin(rad), std::cos(rad));
                auto rotated_pwh = CGAL::transform(rotate_cgal, it->cgal_poly);
                auto r_bb = rotated_pwh.outer_boundary().bbox();
                packaide::Transformation normalize(CGAL::TRANSLATION, packaide::Vector_2(-r_bb.xmin(), -r_bb.ymin()));
                rotated_pwh = CGAL::transform(normalize, rotated_pwh);
                
                packaide::Transformation final_tr(CGAL::TRANSLATION, packaide::Vector_2(world_x, world_y));
                auto placed_poly = CGAL::transform(final_tr, rotated_pwh);
                
                if (config.spacing > packaide::FT(0)) {
                    auto square = packaide::create_offset_square(config.spacing);
                    auto consumed_poly = CGAL::minkowski_sum_2(placed_poly, square);
                    sheet_remainder.difference(consumed_poly);
                } else {
                    sheet_remainder.difference(placed_poly);
                }
            }

            Geometry remainder_geo;
            std::vector<packaide::Polygon_with_holes_2> pwhs; 
            sheet_remainder.polygons_with_holes(std::back_inserter(pwhs));
            std::cout << "[Packaide] Remainder Islands: " << pwhs.size() << std::endl;
            for (size_t i = 0; i < pwhs.size(); ++i) {
                const auto& pwh = pwhs[i];
                Geometry::Face face; 
                std::vector<int> outer;
                
                auto poly_outer = pwh.outer_boundary();
                bool is_ccw = poly_outer.is_counterclockwise_oriented();
                std::cout << "  [Island " << i << "] Outer CCW: " << is_ccw << " (Vertices: " << poly_outer.size() << ")" << std::endl;

                for (auto it = poly_outer.vertices_begin(); it != poly_outer.vertices_end(); ++it) { 
                    outer.push_back((int)remainder_geo.vertices.size()); 
                    remainder_geo.vertices.push_back({FT(it->x()), FT(it->y()), FT(0)}); 
                }
                if (!outer.empty()) face.loops.push_back(outer);
                
                int hole_idx = 0;
                for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) { 
                    std::cout << "    [Hole " << hole_idx++ << "] CCW: " << hit->is_counterclockwise_oriented() << " (Vertices: " << hit->size() << ")" << std::endl;
                    std::vector<int> hole; 
                    for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) { 
                        hole.push_back((int)remainder_geo.vertices.size()); 
                        remainder_geo.vertices.push_back({FT(it->x()), FT(it->y()), FT(0)}); 
                    } 
                    if (!hole.empty()) face.loops.push_back(hole); 
                }
                remainder_geo.faces.push_back(face);
            }
            result.remainders.push_back(remainder_geo);

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
