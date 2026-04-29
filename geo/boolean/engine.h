#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/intersections.h>
#include <vector>
#include <algorithm>
#include <variant>
#include "kernel.h"
#include "../fix/repair.h"

namespace jotcad {
namespace geo {
namespace boolean {

typedef CGAL::Surface_mesh<EK::Point_3> Surface_mesh;
typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

struct Engine {
    /**
     * cut_mesh_by_mesh: 3D Volume-Volume subtraction.
     */
    static bool cut_mesh_by_mesh(Surface_mesh& target, Surface_mesh& tool) {
        if (!CGAL::is_closed(target) || !CGAL::is_closed(tool)) {
            return CGAL::Polygon_mesh_processing::clip(
                target, tool, 
                CGAL::parameters::use_compact_clipper(true).clip_volume(false)
            );
        }

        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );

        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * join_mesh_by_mesh: 3D Volume-Volume union.
     */
    static bool join_mesh_by_mesh(Surface_mesh& target, Surface_mesh& tool) {
        if (!CGAL::is_closed(target) || !CGAL::is_closed(tool)) {
            // Cannot union open meshes easily in 3D without clear inside/outside.
            // For now, we return false or just merge if they don't overlap properly.
            return false;
        }

        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );

        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * clip_mesh_by_mesh: 3D Volume-Volume intersection.
     */
    static bool clip_mesh_by_mesh(Surface_mesh& target, Surface_mesh& tool) {
        if (!CGAL::is_closed(target) || !CGAL::is_closed(tool)) {
            return false;
        }

        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );

        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * cut_mesh_by_plane: 3D Mesh split by infinite plane.
     */
    static bool cut_mesh_by_plane(Surface_mesh& target, const EK::Plane_3& plane) {
        bool success = CGAL::Polygon_mesh_processing::clip(
            target, plane,
            CGAL::parameters::use_compact_clipper(true).clip_volume(true)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * cut_points_by_mesh: 0D Point removal by 3D Volume.
     */
    static void cut_points_by_mesh(std::vector<EK::Point_3>& points, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) return; 
        
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return inside_check(p) == CGAL::ON_BOUNDED_SIDE;
                }),
            points.end()
        );
    }

    /**
     * clip_points_by_mesh: Keeps only points INSIDE the tool.
     */
    static void clip_points_by_mesh(std::vector<EK::Point_3>& points, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) {
            points.clear();
            return;
        }
        
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return inside_check(p) != CGAL::ON_BOUNDED_SIDE;
                }),
            points.end()
        );
    }

    /**
     * cut_segments_by_mesh: 1D Segment clipping by 3D Volume.
     * Keeps portions of segments that are OUTSIDE the tool.
     */
    static void cut_segments_by_mesh(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) return;

        typedef CGAL::AABB_face_graph_triangle_primitive<Surface_mesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        
        Tree tree(faces(tool).first, faces(tool).second, tool);
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);

        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;

        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<Tree::Intersection_and_primitive_id<EK::Segment_3>::Type> intersections;
            tree.all_intersections(s, std::back_inserter(intersections));

            if (intersections.empty()) {
                if (inside_check(seg.first) != CGAL::ON_BOUNDED_SIDE) {
                    result.push_back(seg);
                }
                continue;
            }

            std::vector<EK::Point_3> split_pts;
            split_pts.push_back(seg.first);
            for (auto const& inter : intersections) {
                // Access using std::get_if for std::variant results
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&inter.first)) {
                    split_pts.push_back(*p);
                } else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&inter.first)) {
                    split_pts.push_back(s_inter->source());
                    split_pts.push_back(s_inter->target());
                }
            }
            split_pts.push_back(seg.second);

            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) {
                return (a - seg.first) * dir < (b - seg.first) * dir;
            });

            for (size_t i = 0; i < split_pts.size() - 1; ++i) {
                if (split_pts[i] == split_pts[i+1]) continue;
                EK::Point_3 mid = CGAL::midpoint(split_pts[i], split_pts[i+1]);
                if (inside_check(mid) != CGAL::ON_BOUNDED_SIDE) {
                    result.push_back({split_pts[i], split_pts[i+1]});
                }
            }
        }
        segments = std::move(result);
    }

    /**
     * clip_segments_by_mesh: Keeps only portions of segments INSIDE the tool.
     */
    static void clip_segments_by_mesh(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) {
            segments.clear();
            return;
        }

        typedef CGAL::AABB_face_graph_triangle_primitive<Surface_mesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        
        Tree tree(faces(tool).first, faces(tool).second, tool);
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);

        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;

        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<Tree::Intersection_and_primitive_id<EK::Segment_3>::Type> intersections;
            tree.all_intersections(s, std::back_inserter(intersections));

            if (intersections.empty()) {
                if (inside_check(seg.first) == CGAL::ON_BOUNDED_SIDE) {
                    result.push_back(seg);
                }
                continue;
            }

            std::vector<EK::Point_3> split_pts;
            split_pts.push_back(seg.first);
            for (auto const& inter : intersections) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&inter.first)) {
                    split_pts.push_back(*p);
                } else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&inter.first)) {
                    split_pts.push_back(s_inter->source());
                    split_pts.push_back(s_inter->target());
                }
            }
            split_pts.push_back(seg.second);

            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) {
                return (a - seg.first) * dir < (b - seg.first) * dir;
            });

            for (size_t i = 0; i < split_pts.size() - 1; ++i) {
                if (split_pts[i] == split_pts[i+1]) continue;
                EK::Point_3 mid = CGAL::midpoint(split_pts[i], split_pts[i+1]);
                if (inside_check(mid) == CGAL::ON_BOUNDED_SIDE) {
                    result.push_back({split_pts[i], split_pts[i+1]});
                }
            }
        }
        segments = std::move(result);
    }

    /**
     * cut_gps_by_gps: 2D Polygon-Polygon subtraction.
     */
    static void cut_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) {
        target.difference(tool);
    }

    /**
     * join_gps_by_gps: 2D Polygon-Polygon union.
     */
    static void join_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) {
        target.join(tool);
    }

    /**
     * clip_gps_by_gps: 2D Polygon-Polygon intersection.
     */
    static void clip_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) {
        target.intersection(tool);
    }

    static Surface_mesh geometry_to_mesh(const Geometry& geo) {
        std::vector<EK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
        for (const auto& f : geo.faces) {
            if (f.loops.empty()) continue;
            std::vector<std::size_t> face;
            for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
            faces.push_back(face);
        }
        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
        Surface_mesh mesh;
        std::vector<Surface_mesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f : faces) {
            std::vector<Surface_mesh::Vertex_index> face_vs;
            for (auto idx : f) face_vs.push_back(v_indices[idx]);
            mesh.add_face(face_vs);
        }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry(const Surface_mesh& mesh) {
        Geometry geo;
        std::map<Surface_mesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) {
            v_map[v] = (int)geo.vertices.size();
            auto p = mesh.point(v);
            geo.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for (auto f : mesh.faces()) {
            Geometry::Face face;
            std::vector<int> loop;
            for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]);
            face.loops.push_back(loop);
            geo.faces.push_back(face);
        }
        return geo;
    }

    static void transform_mesh(Surface_mesh& mesh, const Matrix& tf) {
        for (auto v : mesh.vertices()) mesh.point(v) = tf.transform(mesh.point(v));
    }

    static void add_geometry_to_gps(const Geometry& geo, const Matrix& tf, General_polygon_set_2& gps) {
        Geometry t = geo;
        t.apply_tf(tf);
        for (const auto& face : t.faces) {
            if (face.loops.empty()) continue;
            Polygon_2 boundary;
            for (int idx : face.loops[0]) boundary.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
            
            try {
                if (boundary.is_empty() || !boundary.is_simple()) continue;
                if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();
                
                std::vector<Polygon_2> holes;
                for (size_t i = 1; i < face.loops.size(); ++i) {
                    Polygon_2 h;
                    for (int idx : face.loops[i]) h.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
                    if (h.is_simple()) {
                        if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                        holes.push_back(h);
                    }
                }
                gps.join(Polygon_with_holes_2(boundary, holes.begin(), holes.end()));
            } catch (...) {
                // Skip invalid polygons gracefully
            }
        }
    }

    static Geometry gps_to_geometry(const General_polygon_set_2& gps) {
        Geometry g;
        std::vector<Polygon_with_holes_2> pwhs;
        gps.polygons_with_holes(std::back_inserter(pwhs));
        for (const auto& pwh : pwhs) {
            Geometry::Face face;
            std::vector<int> outer;
            for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                outer.push_back((int)g.vertices.size());
                g.vertices.push_back({it->x(), it->y(), FT(0)});
            }
            if (!outer.empty()) face.loops.push_back(outer);
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                std::vector<int> hole;
                for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                    hole.push_back((int)g.vertices.size());
                    g.vertices.push_back({it->x(), it->y(), FT(0)});
                }
                if (!hole.empty()) face.loops.push_back(hole);
            }
            g.faces.push_back(face);
        }
        return g;
    }
};

} // namespace boolean
} // namespace geo
} // namespace jotcad
