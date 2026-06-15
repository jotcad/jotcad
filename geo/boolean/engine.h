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
#include <map>
#include <set>
#include <cassert>
#include <iterator>
#include "kernel.h"
#include "../fix/repair.h"
#include "../data/geometry.h"
#include "../data/shape.h"
#include "../math/matrix.h"
#include "../../fs/cpp/vfs_node.h"

namespace jotcad {
namespace geo {
namespace boolean {

typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef ExactMesh Surface_mesh; // Alias for compatibility
typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;
typedef InexactMesh InexactSurfaceMesh; // Alias for compatibility
typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

struct Engine {
    /**
     * cut_mesh_by_mesh: 3D Volume-Volume subtraction OR Surface-Volume subtraction.
     */
    static bool cut_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    static void cut_surface_by_solid(ExactMesh& target, const ExactMesh& tool) {
        if (CGAL::is_closed(tool)) {
            ExactMesh tool_copy = tool; // split needs non-const splitter
            CGAL::Polygon_mesh_processing::split(target, tool_copy);
            CGAL::Side_of_triangle_mesh<ExactMesh, EK> inside_check(tool_copy);
            std::vector<ExactMesh::Face_index> to_remove;
            for (auto f : target.faces()) {
                auto h = target.halfedge(f);
                auto p0 = target.point(target.source(h));
                auto p1 = target.point(target.target(h));
                auto p2 = target.point(target.target(target.next(h)));
                auto mid = CGAL::centroid(p0, p1, p2);
                if (inside_check(mid) != CGAL::ON_UNBOUNDED_SIDE) {
                    to_remove.push_back(f);
                }
            }
            for (auto f : to_remove) {
                CGAL::Euler::remove_face(target.halfedge(f), target);
            }
            CGAL::Polygon_mesh_processing::remove_isolated_vertices(target);
        }
    }

    /**
     * join_mesh_by_mesh: 3D Volume-Volume union.
     */
    static bool join_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
        if (!CGAL::is_closed(target) || !CGAL::is_closed(tool)) return false;
        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * clip_mesh_by_mesh: 3D Volume-Volume intersection OR Surface-Volume intersection.
     */
    static bool clip_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    static bool cut_mesh_by_plane(ExactMesh& target, const EK::Plane_3& plane) {
        bool success = CGAL::Polygon_mesh_processing::clip(
            target, plane.opposite(),
            CGAL::parameters::use_compact_clipper(true).clip_volume(true)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    static bool clip_mesh_by_plane(ExactMesh& target, const EK::Plane_3& plane) {
        bool success = CGAL::Polygon_mesh_processing::clip(
            target, plane,
            CGAL::parameters::use_compact_clipper(true).clip_volume(true)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    // --- Points Booleans ---

    static void cut_points_by_mesh(std::vector<EK::Point_3>& points, const ExactMesh& tool) {
        if (points.empty()) return;
        bool is_closed = CGAL::is_closed(tool);
        typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        Tree tree(faces(tool).first, faces(tool).second, tool);
        std::unique_ptr<CGAL::Side_of_triangle_mesh<ExactMesh, EK>> inside_check;
        if (is_closed) inside_check = std::make_unique<CGAL::Side_of_triangle_mesh<ExactMesh, EK>>(tool);

        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            if (is_closed) return inside_check->operator()(p) != CGAL::ON_UNBOUNDED_SIDE;
            return tree.do_intersect(p);
        }), points.end());
    }

    static void cut_points_by_plane(std::vector<EK::Point_3>& points, const EK::Plane_3& plane) {
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            return plane.has_on_negative_side(p);
        }), points.end());
    }

    static void cut_points_by_points(std::vector<EK::Point_3>& points, const std::vector<EK::Point_3>& tool_points) {
        std::set<EK::Point_3> tool_set(tool_points.begin(), tool_points.end());
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            return tool_set.count(p) > 0;
        }), points.end());
    }

    static void cut_points_by_segments(std::vector<EK::Point_3>& points, const std::vector<std::pair<EK::Point_3, EK::Point_3>>& tool_segments) {
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            for (const auto& s : tool_segments) if (EK::Segment_3(s.first, s.second).has_on(p)) return true;
            return false;
        }), points.end());
    }

    static void clip_points_by_mesh(std::vector<EK::Point_3>& points, const ExactMesh& tool) {
        if (points.empty()) return;
        bool is_closed = CGAL::is_closed(tool);
        typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        Tree tree(faces(tool).first, faces(tool).second, tool);
        std::unique_ptr<CGAL::Side_of_triangle_mesh<ExactMesh, EK>> inside_check;
        if (is_closed) inside_check = std::make_unique<CGAL::Side_of_triangle_mesh<ExactMesh, EK>>(tool);

        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            if (is_closed) return inside_check->operator()(p) == CGAL::ON_UNBOUNDED_SIDE;
            return !tree.do_intersect(p);
        }), points.end());
    }

    static void clip_points_by_plane(std::vector<EK::Point_3>& points, const EK::Plane_3& plane) {
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            return !plane.has_on_negative_side(p);
        }), points.end());
    }

    static void clip_points_by_points(std::vector<EK::Point_3>& points, const std::vector<EK::Point_3>& tool_points) {
        std::set<EK::Point_3> tool_set(tool_points.begin(), tool_points.end());
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            return tool_set.count(p) == 0;
        }), points.end());
    }

    static void clip_points_by_segments(std::vector<EK::Point_3>& points, const std::vector<std::pair<EK::Point_3, EK::Point_3>>& tool_segments) {
        points.erase(std::remove_if(points.begin(), points.end(), [&](const EK::Point_3& p) {
            for (const auto& s : tool_segments) if (EK::Segment_3(s.first, s.second).has_on(p)) return false;
            return true;
        }), points.end());
    }

    static void join_points_by_points(std::vector<EK::Point_3>& target, const std::vector<EK::Point_3>& tool) {
        std::set<EK::Point_3> existing(target.begin(), target.end());
        for (const auto& p : tool) if (existing.count(p) == 0) { target.push_back(p); existing.insert(p); }
    }

    // --- Segments Booleans ---

    static void cut_segments_by_mesh(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const ExactMesh& tool, bool is_closed) {
        if (segments.empty()) return;
        typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        Tree tree(faces(tool).first, faces(tool).second, tool);
        std::unique_ptr<CGAL::Side_of_triangle_mesh<ExactMesh, EK>> inside_check;
        if (is_closed) inside_check = std::make_unique<CGAL::Side_of_triangle_mesh<ExactMesh, EK>>(tool);

        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<Tree::Intersection_and_primitive_id<EK::Segment_3>::Type> intersections;
            tree.all_intersections(s, std::back_inserter(intersections));

            std::vector<EK::Point_3> split_pts = {seg.first, seg.second};
            for (auto const& inter : intersections) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&inter.first)) split_pts.push_back(*p);
                else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&inter.first)) { split_pts.push_back(s_inter->source()); split_pts.push_back(s_inter->target()); }
            }
            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) { return (a - seg.first) * dir < (b - seg.first) * dir; });

            for (size_t i = 0; i < split_pts.size() - 1; ++i) {
                if (split_pts[i] == split_pts[i+1]) continue;
                EK::Point_3 mid = CGAL::midpoint(split_pts[i], split_pts[i+1]);
                bool is_inside = is_closed ? (inside_check->operator()(mid) != CGAL::ON_UNBOUNDED_SIDE) : tree.do_intersect(mid);
                if (!is_inside) result.push_back({split_pts[i], split_pts[i+1]});
            }
        }
        segments = std::move(result);
    }

    static void cut_segments_by_plane(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const EK::Plane_3& plane) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            auto inter = CGAL::intersection(s, plane);
            if (inter) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&*inter)) {
                    if (!plane.has_on_negative_side(seg.first)) result.push_back({seg.first, *p});
                    if (!plane.has_on_negative_side(seg.second)) result.push_back({*p, seg.second});
                } else result.push_back(seg);
            } else if (!plane.has_on_negative_side(seg.first)) result.push_back(seg);
        }
        segments = std::move(result);
    }

    static void cut_segments_by_points(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const std::vector<EK::Point_3>& points) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<EK::Point_3> split_pts = {seg.first, seg.second};
            for (const auto& p : points) if (s.has_on(p)) split_pts.push_back(p);
            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) { return (a - seg.first) * dir < (b - seg.first) * dir; });
            for (size_t i = 0; i < split_pts.size() - 1; ++i) if (split_pts[i] != split_pts[i+1]) result.push_back({split_pts[i], split_pts[i+1]});
        }
        segments = std::move(result);
    }

    static void cut_segments_by_segments(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const std::vector<std::pair<EK::Point_3, EK::Point_3>>& tool_segments) {
        for (const auto& ts_pair : tool_segments) {
            EK::Segment_3 ts(ts_pair.first, ts_pair.second);
            std::vector<std::pair<EK::Point_3, EK::Point_3>> next;
            for (const auto& s_pair : segments) {
                EK::Segment_3 s(s_pair.first, s_pair.second);
                auto inter = CGAL::intersection(s, ts);
                if (inter) {
                    if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&*inter)) {
                        std::vector<EK::Point_3> pts = {s_pair.first, s_pair.second, s_inter->source(), s_inter->target()};
                        EK::Vector_3 dir = s_pair.second - s_pair.first;
                        std::sort(pts.begin(), pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) { return (a - s_pair.first) * dir < (b - s_pair.first) * dir; });
                        for (size_t i = 0; i < pts.size() - 1; ++i) {
                            if (pts[i] == pts[i+1]) continue;
                            if (!ts.has_on(CGAL::midpoint(pts[i], pts[i+1]))) next.push_back({pts[i], pts[i+1]});
                        }
                    } else next.push_back(s_pair);
                } else next.push_back(s_pair);
            }
            segments = std::move(next);
        }
    }

    static void clip_segments_by_mesh(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const ExactMesh& tool, bool is_closed) {
        if (segments.empty()) return;
        typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        Tree tree(faces(tool).first, faces(tool).second, tool);
        std::unique_ptr<CGAL::Side_of_triangle_mesh<ExactMesh, EK>> inside_check;
        if (is_closed) inside_check = std::make_unique<CGAL::Side_of_triangle_mesh<ExactMesh, EK>>(tool);

        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<Tree::Intersection_and_primitive_id<EK::Segment_3>::Type> intersections;
            tree.all_intersections(s, std::back_inserter(intersections));

            std::vector<EK::Point_3> split_pts = {seg.first, seg.second};
            for (auto const& inter : intersections) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&inter.first)) split_pts.push_back(*p);
                else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&inter.first)) { split_pts.push_back(s_inter->source()); split_pts.push_back(s_inter->target()); }
            }
            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) { return (a - seg.first) * dir < (b - seg.first) * dir; });

            for (size_t i = 0; i < split_pts.size() - 1; ++i) {
                if (split_pts[i] == split_pts[i+1]) continue;
                EK::Point_3 mid = CGAL::midpoint(split_pts[i], split_pts[i+1]);
                bool is_inside = is_closed ? (inside_check->operator()(mid) != CGAL::ON_UNBOUNDED_SIDE) : tree.do_intersect(mid);
                if (is_inside) result.push_back({split_pts[i], split_pts[i+1]});
            }
        }
        segments = std::move(result);
    }

    static void clip_segments_by_plane(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const EK::Plane_3& plane) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            auto inter = CGAL::intersection(s, plane);
            if (inter) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&*inter)) {
                    if (plane.has_on_negative_side(seg.first)) result.push_back({seg.first, *p});
                    if (plane.has_on_negative_side(seg.second)) result.push_back({*p, seg.second});
                } else result.push_back(seg);
            } else if (plane.has_on_negative_side(seg.first)) result.push_back(seg);
        }
        segments = std::move(result);
    }

    static void clip_segments_by_segments(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const std::vector<std::pair<EK::Point_3, EK::Point_3>>& tool_segments) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& s_pair : segments) {
            EK::Segment_3 s(s_pair.first, s_pair.second);
            for (const auto& ts_pair : tool_segments) {
                EK::Segment_3 ts(ts_pair.first, ts_pair.second);
                auto inter = CGAL::intersection(s, ts);
                if (inter) if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&*inter)) result.push_back({s_inter->source(), s_inter->target()});
            }
        }
        segments = std::move(result);
    }

    static void clip_segments_by_points(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const std::vector<EK::Point_3>& tool_points) { segments.clear(); }

    static void join_segments_by_segments(std::vector<std::pair<EK::Point_3, EK::Point_3>>& target, const std::vector<std::pair<EK::Point_3, EK::Point_3>>& tool) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> pool = target;
        pool.insert(pool.end(), tool.begin(), tool.end());
        
        struct LineKey {
            EK::Vector_3 dir;
            EK::Point_3 origin;
            bool operator<(const LineKey& other) const {
                if (dir.x() != other.dir.x()) return dir.x() < other.dir.x();
                if (dir.y() != other.dir.y()) return dir.y() < other.dir.y();
                if (dir.z() != other.dir.z()) return dir.z() < other.dir.z();
                return CGAL::compare_xyz(origin, other.origin) == CGAL::SMALLER;
            }
        };

        auto normalize_line = [](const EK::Segment_3& s) -> LineKey {
            EK::Vector_3 v = s.to_vector();
            if (v.x() < 0 || (v.x() == 0 && v.y() < 0) || (v.x() == 0 && v.y() == 0 && v.z() < 0)) v = -v;
            if (v.x() != 0) { auto f = v.x(); v = EK::Vector_3(1, v.y()/f, v.z()/f); }
            else if (v.y() != 0) { auto f = v.y(); v = EK::Vector_3(0, 1, v.z()/f); }
            else if (v.z() != 0) { auto f = v.z(); v = EK::Vector_3(0, 0, 1); }
            
            EK::Point_3 p0(0,0,0);
            EK::Vector_3 source_vec = s.source() - p0;
            auto t = -(source_vec * v) / (v * v);
            EK::Point_3 origin = s.source() + t * v;
            return {v, origin};
        };

        std::map<LineKey, std::vector<std::pair<EK::FT, EK::FT>>> line_intervals;
        std::vector<EK::Point_3> degenerate;

        for (const auto& p : pool) {
            if (p.first == p.second) { degenerate.push_back(p.first); continue; }
            EK::Segment_3 s(p.first, p.second);
            LineKey key = normalize_line(s);
            auto t1 = (s.source() - key.origin) * key.dir;
            auto t2 = (s.target() - key.origin) * key.dir;
            if (t1 > t2) std::swap(t1, t2);
            line_intervals[key].push_back({t1, t2});
        }

        target.clear();
        for (auto& entry : line_intervals) {
            auto& intervals = entry.second;
            std::sort(intervals.begin(), intervals.end());
            
            std::vector<std::pair<EK::FT, EK::FT>> merged;
            for (const auto& interval : intervals) {
                if (merged.empty() || interval.first > merged.back().second) merged.push_back(interval);
                else merged.back().second = std::max(merged.back().second, interval.second);
            }
            
            auto d2 = entry.first.dir * entry.first.dir;
            for (const auto& m : merged) {
                target.push_back({
                    entry.first.origin + (m.first / d2) * entry.first.dir,
                    entry.first.origin + (m.second / d2) * entry.first.dir
                });
            }
        }
        
        std::set<EK::Point_3> seen_degenerate;
        for (const auto& p : degenerate) {
            if (seen_degenerate.count(p) == 0) {
                bool contained = false;
                for (const auto& s : target) if (EK::Segment_3(s.first, s.second).has_on(p)) { contained = true; break; }
                if (!contained) { target.push_back({p, p}); seen_degenerate.insert(p); }
            }
        }
    }

    // --- GPS Utils ---

    static void cut_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) { target.difference(tool); }
    static void join_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) { target.join(tool); }
    static void clip_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) { target.intersection(tool); }

    // --- Mesh Conversion ---

    static ExactMesh geometry_to_mesh(const Geometry& geo) {
        std::vector<EK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
        if (!geo.triangles.empty()) for (const auto& t : geo.triangles) faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
        else for (const auto& f : geo.faces) if (!f.loops.empty()) { std::vector<std::size_t> face; for (int idx : f.loops[0]) face.push_back((std::size_t)idx); faces.push_back(face); }
        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
        ExactMesh mesh;
        std::vector<ExactMesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f_indices : faces) { std::vector<ExactMesh::Vertex_index> face_vs; for (auto idx : f_indices) face_vs.push_back(v_indices[idx]); mesh.add_face(face_vs); }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry(const ExactMesh& mesh) {
        Geometry geo;
        std::map<ExactMesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) { v_map[v] = (int)geo.vertices.size(); auto p = mesh.point(v); geo.vertices.push_back({p.x(), p.y(), p.z()}); }
        for (auto f : mesh.faces()) { std::vector<int> loop; for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]); assert(loop.size() == 3); geo.triangles.push_back({loop[0], loop[1], loop[2]}); }
        return geo;
    }

    static InexactMesh geometry_to_mesh_ik(const Geometry& geo) {
        std::vector<IK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(IK::Point_3(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)));
        if (!geo.triangles.empty()) for (const auto& t : geo.triangles) faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
        else for (const auto& f : geo.faces) if (!f.loops.empty()) { std::vector<std::size_t> face; for (int idx : f.loops[0]) face.push_back((std::size_t)idx); faces.push_back(face); }
        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
        InexactMesh mesh;
        std::vector<InexactMesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f_indices : faces) { std::vector<InexactMesh::Vertex_index> face_vs; for (auto idx : f_indices) face_vs.push_back(v_indices[idx]); mesh.add_face(face_vs); }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry_ik(const InexactMesh& mesh) {
        Geometry geo;
        std::map<InexactMesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) { v_map[v] = (int)geo.vertices.size(); auto p = mesh.point(v); geo.vertices.push_back({CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())}); }
        for (auto f : mesh.faces()) { std::vector<int> loop; for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]); assert(loop.size() == 3); geo.triangles.push_back({loop[0], loop[1], loop[2]}); }
        return geo;
    }

    static void transform_mesh(ExactMesh& mesh, const Matrix& tf) { for (auto v : mesh.vertices()) mesh.point(v) = tf.transform(mesh.point(v)); }

    static void add_geometry_to_gps(const Geometry& geo, const Matrix& tf, General_polygon_set_2& gps) {
        Geometry t = geo; t.apply_tf(tf);
        for (const auto& face : t.faces) {
            if (face.loops.empty()) continue;
            Polygon_2 boundary; for (int idx : face.loops[0]) boundary.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
            try {
                if (boundary.is_empty() || !boundary.is_simple()) continue;
                if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();
                std::vector<Polygon_2> holes;
                for (size_t i = 1; i < face.loops.size(); ++i) { Polygon_2 h; for (int idx : face.loops[i]) h.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y)); if (h.is_simple()) { if (h.is_counterclockwise_oriented()) h.reverse_orientation(); holes.push_back(h); } }
                gps.join(Polygon_with_holes_2(boundary, holes.begin(), holes.end()));
            } catch (...) {}
        }
    }

    static Geometry gps_to_geometry(const General_polygon_set_2& gps) {
        Geometry g;
        std::vector<Polygon_with_holes_2> pwhs; gps.polygons_with_holes(std::back_inserter(pwhs));
        for (const auto& pwh : pwhs) {
            Geometry::Face face; std::vector<int> outer;
            for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) { outer.push_back((int)g.vertices.size()); g.vertices.push_back({it->x(), it->y(), FT(0)}); }
            if (!outer.empty()) face.loops.push_back(outer);
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) { std::vector<int> hole; for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) { hole.push_back((int)g.vertices.size()); g.vertices.push_back({it->x(), it->y(), FT(0)}); } if (!hole.empty()) face.loops.push_back(hole); }
            g.faces.push_back(face);
        }
        return g;
    }

    struct ToolNode { Geometry geo; Matrix world_tf; std::string type; bool is_gap = false; };

    static void collect_tool_geometry(fs::VFSNode* vfs, const Shape& s, const Matrix& /*ignored_parent_tf*/, std::vector<ToolNode>& tool_nodes) {
        if (s.is_ghost() || s.is_mark() || s.is_mask()) return;
        Matrix current_tf = s.tf;
        std::string type = s.tags.value("type", "");
        bool is_gap = s.is_gap();
        if (s.geometry.has_value()) tool_nodes.push_back({vfs->read<Geometry>(s.geometry.value()), current_tf, type, is_gap});
        else if (type == "plane") tool_nodes.push_back({Geometry(), current_tf, type, is_gap});
        for (const auto& child : s.components) collect_tool_geometry(vfs, child, current_tf /* unused */, tool_nodes);
    }

    // --- Recursive Boolean Orchestrators ---

    static void recursive_subtract(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes, bool open, bool stamp = false) {
        if (!s.is_solid() && !s.is_gap()) return;
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value() && !s.is_gap()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_points = !target_geo.points.empty();

            if (has_faces) {
                bool original_is_flat = target_geo.is_plane();
                bool is_target_flat = original_is_flat;
                if (is_target_flat && !stamp) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();
                    General_polygon_set_2 subject_set; add_geometry_to_gps(target_geo, project_tf, subject_set);
                    bool used_pwh_path = false;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; local_tool_geo.apply_tf(tool_rel_tf);
                        if (local_tool_geo.is_coplanar_with(target_plane)) { General_polygon_set_2 tool_set; add_geometry_to_gps(local_tool_geo, project_tf, tool_set); cut_gps_by_gps(subject_set, tool_set); used_pwh_path = true; }
                        else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { used_pwh_path = false; break; }
                    }
                    if (used_pwh_path) { target_geo = gps_to_geometry(subject_set); target_geo.apply_tf(rehydrate_tf); }
                    else is_target_flat = false;
                }
                if (!is_target_flat || (original_is_flat && stamp)) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        if (tool.type == "plane") cut_mesh_by_plane(target_mesh, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                        else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { 
                            ExactMesh tool_mesh = geometry_to_mesh(tool.geo); 
                            transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); 
                            if (original_is_flat && !stamp) {
                                cut_surface_by_solid(target_mesh, tool_mesh);
                            } else {
                                cut_mesh_by_mesh(target_mesh, tool_mesh); 
                            }
                        }
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            }
            if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) local_segments.push_back({EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z), EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)});
                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); cut_segments_by_mesh(local_segments, tool_mesh, tool.type == "closed"); }
                    else if (tool.type == "plane") cut_segments_by_plane(local_segments, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                    else if (tool.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> tool_segs; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (const auto& seg : tool.geo.segments) tool_segs.push_back({tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[0]].x, tool.geo.vertices[seg[0]].y, tool.geo.vertices[seg[0]].z)), tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[1]].x, tool.geo.vertices[seg[1]].y, tool.geo.vertices[seg[1]].z))}); cut_segments_by_segments(local_segments, tool_segs); }
                    else if (tool.type == "point" || tool.type == "points") { std::vector<EK::Point_3> tool_pts; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (int idx : tool.geo.points) tool_pts.push_back(tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[idx].x, tool.geo.vertices[idx].y, tool.geo.vertices[idx].z))); cut_segments_by_points(local_segments, tool_pts); }
                }
                target_geo.segments.clear(); if (!has_faces && !has_points) target_geo.vertices.clear();
                for (const auto& seg : local_segments) { int v1 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()}); int v2 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()}); target_geo.segments.push_back({v1, v2}); }
            }
            if (has_points) {
                std::vector<EK::Point_3> pts; for (int idx : target_geo.points) pts.push_back(EK::Point_3(target_geo.vertices[idx].x, target_geo.vertices[idx].y, target_geo.vertices[idx].z));
                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); cut_points_by_mesh(pts, tool_mesh); }
                    else if (tool.type == "plane") cut_points_by_plane(pts, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                    else if (tool.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> tool_segs; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (const auto& seg : tool.geo.segments) tool_segs.push_back({tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[0]].x, tool.geo.vertices[seg[0]].y, tool.geo.vertices[seg[0]].z)), tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[1]].x, tool.geo.vertices[seg[1]].y, tool.geo.vertices[seg[1]].z))}); cut_points_by_segments(pts, tool_segs); }
                    else if (tool.type == "point" || tool.type == "points") { std::vector<EK::Point_3> tool_pts; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (int idx : tool.geo.points) tool_pts.push_back(tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[idx].x, tool.geo.vertices[idx].y, tool.geo.vertices[idx].z))); cut_points_by_points(pts, tool_pts); }
                }
                target_geo.points.clear(); if (!has_faces && !has_segments) target_geo.vertices.clear();
                for (const auto& p : pts) { target_geo.points.push_back((int)target_geo.vertices.size()); target_geo.vertices.push_back({p.x(), p.y(), p.z()}); }
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }
        for (auto& child : s.components) recursive_subtract(vfs, child, subject_world_tf, tool_nodes, open, stamp);
    }

    static void recursive_union(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        if (!s.is_solid() && !s.is_gap()) return;
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value() && !s.is_gap()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_points = !target_geo.points.empty();

            std::vector<ToolNode> regular_tools, gap_tools;
            for (const auto& t : tool_nodes) { if (t.is_gap) gap_tools.push_back(t); else regular_tools.push_back(t); }

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();
                if (is_target_flat) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();
                    General_polygon_set_2 subject_set; add_geometry_to_gps(target_geo, project_tf, subject_set);
                    bool used_pwh_path = true;
                    for (const auto& tool : regular_tools) {
                        Geometry local_tool_geo = tool.geo; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; local_tool_geo.apply_tf(tool_rel_tf);
                        if (local_tool_geo.is_coplanar_with(target_plane)) { General_polygon_set_2 tool_set; add_geometry_to_gps(local_tool_geo, project_tf, tool_set); join_gps_by_gps(subject_set, tool_set); }
                        else { used_pwh_path = false; break; }
                    }
                    if (used_pwh_path) {
                        for (const auto& gap : gap_tools) {
                            Geometry local_gap_geo = gap.geo; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; local_gap_geo.apply_tf(gap_rel_tf);
                            if (local_gap_geo.is_coplanar_with(target_plane)) { General_polygon_set_2 gap_set; add_geometry_to_gps(local_gap_geo, project_tf, gap_set); cut_gps_by_gps(subject_set, gap_set); }
                        }
                        target_geo = gps_to_geometry(subject_set); target_geo.apply_tf(rehydrate_tf);
                    } else is_target_flat = false;
                }
                if (!is_target_flat) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : regular_tools) if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); join_mesh_by_mesh(target_mesh, tool_mesh); }
                    for (const auto& gap : gap_tools) if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_mesh_by_mesh(target_mesh, gap_mesh); }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            }
            if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) local_segments.push_back({EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z), EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)});
                for (const auto& tool : regular_tools) if (tool.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> tool_segs; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (const auto& seg : tool.geo.segments) tool_segs.push_back({tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[0]].x, tool.geo.vertices[seg[0]].y, tool.geo.vertices[seg[0]].z)), tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[1]].x, tool.geo.vertices[seg[1]].y, tool.geo.vertices[seg[1]].z))}); join_segments_by_segments(local_segments, tool_segs); }
                for (const auto& gap : gap_tools) {
                    if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_segments_by_mesh(local_segments, gap_mesh, gap.type == "closed"); }
                    else if (gap.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> gap_segs; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; for (const auto& seg : gap.geo.segments) gap_segs.push_back({gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[seg[0]].x, gap.geo.vertices[seg[0]].y, gap.geo.vertices[seg[0]].z)), gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[seg[1]].x, gap.geo.vertices[seg[1]].y, gap.geo.vertices[seg[1]].z))}); cut_segments_by_segments(local_segments, gap_segs); }
                }
                target_geo.segments.clear(); if (!has_faces && !has_points) target_geo.vertices.clear();
                for (const auto& seg : local_segments) { int v1 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()}); int v2 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()}); target_geo.segments.push_back({v1, v2}); }
            }
            if (has_points) {
                std::vector<EK::Point_3> pts; for (int idx : target_geo.points) pts.push_back(EK::Point_3(target_geo.vertices[idx].x, target_geo.vertices[idx].y, target_geo.vertices[idx].z));
                for (const auto& tool : regular_tools) if (tool.type == "point" || tool.type == "points") { std::vector<EK::Point_3> tool_pts; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (int idx : tool.geo.points) tool_pts.push_back(tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[idx].x, tool.geo.vertices[idx].y, tool.geo.vertices[idx].z))); join_points_by_points(pts, tool_pts); }
                for (const auto& gap : gap_tools) {
                    if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_points_by_mesh(pts, gap_mesh); }
                    else if (gap.type == "point" || gap.type == "points") { std::vector<EK::Point_3> gap_pts; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; for (int idx : gap.geo.points) gap_pts.push_back(gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[idx].x, gap.geo.vertices[idx].y, gap.geo.vertices[idx].z))); cut_points_by_points(pts, gap_pts); }
                }
                target_geo.points.clear(); if (!has_faces && !has_segments) target_geo.vertices.clear();
                for (const auto& p : pts) { target_geo.points.push_back((int)target_geo.vertices.size()); target_geo.vertices.push_back({p.x(), p.y(), p.z()}); }
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }
        for (auto& child : s.components) recursive_union(vfs, child, subject_world_tf, tool_nodes);
    }

    static void recursive_intersect(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        if (!s.is_solid() && !s.is_gap()) return;
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value() && !s.is_gap()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_points = !target_geo.points.empty();

            std::vector<ToolNode> regular_tools, gap_tools;
            for (const auto& t : tool_nodes) { if (t.is_gap) gap_tools.push_back(t); else regular_tools.push_back(t); }

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();
                if (is_target_flat) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();
                    General_polygon_set_2 subject_set; add_geometry_to_gps(target_geo, project_tf, subject_set);
                    bool used_pwh_path = true;
                    for (const auto& tool : regular_tools) {
                        Geometry local_tool_geo = tool.geo; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; local_tool_geo.apply_tf(tool_rel_tf);
                        if (local_tool_geo.is_coplanar_with(target_plane)) { General_polygon_set_2 tool_set; add_geometry_to_gps(local_tool_geo, project_tf, tool_set); clip_gps_by_gps(subject_set, tool_set); }
                        else { used_pwh_path = false; break; }
                    }
                    if (used_pwh_path) {
                        for (const auto& gap : gap_tools) {
                            Geometry local_gap_geo = gap.geo; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; local_gap_geo.apply_tf(gap_rel_tf);
                            if (local_gap_geo.is_coplanar_with(target_plane)) { General_polygon_set_2 gap_set; add_geometry_to_gps(local_gap_geo, project_tf, gap_set); cut_gps_by_gps(subject_set, gap_set); }
                        }
                        target_geo = gps_to_geometry(subject_set); target_geo.apply_tf(rehydrate_tf);
                    } else is_target_flat = false;
                }
                if (!is_target_flat) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : regular_tools) {
                        if (tool.type == "plane") clip_mesh_by_plane(target_mesh, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                        else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); clip_mesh_by_mesh(target_mesh, tool_mesh); }
                    }
                    for (const auto& gap : gap_tools) if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_mesh_by_mesh(target_mesh, gap_mesh); }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            }
            if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) local_segments.push_back({EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z), EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)});
                for (const auto& tool : regular_tools) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); clip_segments_by_mesh(local_segments, tool_mesh, tool.type == "closed"); }
                    else if (tool.type == "plane") clip_segments_by_plane(local_segments, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                    else if (tool.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> tool_segs; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (const auto& seg : tool.geo.segments) tool_segs.push_back({tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[0]].x, tool.geo.vertices[seg[0]].y, tool.geo.vertices[seg[0]].z)), tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[1]].x, tool.geo.vertices[seg[1]].y, tool.geo.vertices[seg[1]].z))}); clip_segments_by_segments(local_segments, tool_segs); }
                }
                for (const auto& gap : gap_tools) {
                    if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_segments_by_mesh(local_segments, gap_mesh, gap.type == "closed"); }
                    else if (gap.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> gap_segs; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; for (const auto& seg : gap.geo.segments) gap_segs.push_back({gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[seg[0]].x, gap.geo.vertices[seg[0]].y, gap.geo.vertices[seg[0]].z)), gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[seg[1]].x, gap.geo.vertices[seg[1]].y, gap.geo.vertices[seg[1]].z))}); cut_segments_by_segments(local_segments, gap_segs); }
                }
                target_geo.segments.clear(); if (!has_faces && !has_points) target_geo.vertices.clear();
                for (const auto& seg : local_segments) { int v1 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()}); int v2 = (int)target_geo.vertices.size(); target_geo.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()}); target_geo.segments.push_back({v1, v2}); }
            }
            if (has_points) {
                std::vector<EK::Point_3> pts; for (int idx : target_geo.points) pts.push_back(EK::Point_3(target_geo.vertices[idx].x, target_geo.vertices[idx].y, target_geo.vertices[idx].z));
                for (const auto& tool : regular_tools) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") { ExactMesh tool_mesh = geometry_to_mesh(tool.geo); transform_mesh(tool_mesh, subject_world_inv * tool.world_tf); clip_points_by_mesh(pts, tool_mesh); }
                    else if (tool.type == "plane") clip_points_by_plane(pts, (subject_world_inv * tool.world_tf).transform(EK::Plane_3(0,0,1,0)));
                    else if (tool.type == "segments") { std::vector<std::pair<EK::Point_3, EK::Point_3>> tool_segs; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (const auto& seg : tool.geo.segments) tool_segs.push_back({tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[0]].x, tool.geo.vertices[seg[0]].y, tool.geo.vertices[seg[0]].z)), tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[seg[1]].x, tool.geo.vertices[seg[1]].y, tool.geo.vertices[seg[1]].z))}); clip_points_by_segments(pts, tool_segs); }
                    else if (tool.type == "point" || tool.type == "points") { std::vector<EK::Point_3> tool_pts; Matrix tool_rel_tf = subject_world_inv * tool.world_tf; for (int idx : tool.geo.points) tool_pts.push_back(tool_rel_tf.transform(EK::Point_3(tool.geo.vertices[idx].x, tool.geo.vertices[idx].y, tool.geo.vertices[idx].z))); clip_points_by_points(pts, tool_pts); }
                }
                for (const auto& gap : gap_tools) {
                    if (gap.type == "closed" || gap.type == "open" || gap.type == "surface") { ExactMesh gap_mesh = geometry_to_mesh(gap.geo); transform_mesh(gap_mesh, subject_world_inv * gap.world_tf); cut_points_by_mesh(pts, gap_mesh); }
                    else if (gap.type == "point" || gap.type == "points") { std::vector<EK::Point_3> gap_pts; Matrix gap_rel_tf = subject_world_inv * gap.world_tf; for (int idx : gap.geo.points) gap_pts.push_back(gap_rel_tf.transform(EK::Point_3(gap.geo.vertices[idx].x, gap.geo.vertices[idx].y, gap.geo.vertices[idx].z))); cut_points_by_points(pts, gap_pts); }
                }
                target_geo.points.clear(); if (!has_faces && !has_segments) target_geo.vertices.clear();
                for (const auto& p : pts) { target_geo.points.push_back((int)target_geo.vertices.size()); target_geo.vertices.push_back({p.x(), p.y(), p.z()}); }
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }
        for (auto& child : s.components) recursive_intersect(vfs, child, subject_world_tf, tool_nodes);
    }

    static void deep_disjoint(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf) {
        Matrix world_tf = parent_tf * s.tf;
        if (s.geometry.has_value() && !s.components.empty()) { std::vector<ToolNode> tool_nodes; for (const auto& child : s.components) collect_tool_geometry(vfs, child, world_tf, tool_nodes); recursive_subtract(vfs, s, parent_tf, tool_nodes, false); }
        for (size_t i = 0; i < s.components.size(); ++i) if (i + 1 < s.components.size()) { std::vector<ToolNode> tool_nodes; for (size_t j = i + 1; j < s.components.size(); ++j) collect_tool_geometry(vfs, s.components[j], world_tf, tool_nodes); recursive_subtract(vfs, s.components[i], world_tf, tool_nodes, false); }
        for (auto& child : s.components) deep_disjoint(vfs, child, world_tf);
    }
};

} // namespace boolean
} // namespace geo
} // namespace jotcad
