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
     * cut_mesh_by_mesh: 3D Volume-Volume subtraction.
     */
    static bool cut_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
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
    static bool join_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
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
    static bool clip_mesh_by_mesh(ExactMesh& target, ExactMesh& tool) {
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
     * cut_mesh_by_plane: 3D Mesh subtraction by half-space.
     * Keeps the POSITIVE side (removes the tool's volume).
     */
    static bool cut_mesh_by_plane(ExactMesh& target, const EK::Plane_3& plane) {
        // CGAL clip keeps the negative side, so we flip the plane to keep the positive side
        bool success = CGAL::Polygon_mesh_processing::clip(
            target, plane.opposite(),
            CGAL::parameters::use_compact_clipper(true).clip_volume(true)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * clip_mesh_by_plane: 3D Mesh intersection with half-space.
     * Keeps the NEGATIVE side.
     */
    static bool clip_mesh_by_plane(ExactMesh& target, const EK::Plane_3& plane) {
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
    static void cut_points_by_mesh(std::vector<EK::Point_3>& points, const ExactMesh& tool) {
        if (points.empty()) return;
        if (!CGAL::is_closed(tool)) return; 
        
        CGAL::Side_of_triangle_mesh<ExactMesh, EK> inside_check(tool);
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return inside_check(p) == CGAL::ON_BOUNDED_SIDE;
                }),
            points.end()
        );
    }

    /**
     * cut_points_by_plane: Removes points on the negative side.
     */
    static void cut_points_by_plane(std::vector<EK::Point_3>& points, const EK::Plane_3& plane) {
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return plane.has_on_negative_side(p);
                }),
            points.end()
        );
    }

    /**
     * clip_points_by_plane: Keeps points on the negative side.
     */
    static void clip_points_by_plane(std::vector<EK::Point_3>& points, const EK::Plane_3& plane) {
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return !plane.has_on_negative_side(p);
                }),
            points.end()
        );
    }

    /**
     * clip_points_by_mesh: Keeps only points INSIDE the tool.
     */
    static void clip_points_by_mesh(std::vector<EK::Point_3>& points, const ExactMesh& tool) {
        if (!CGAL::is_closed(tool)) {
            points.clear();
            return;
        }
        
        CGAL::Side_of_triangle_mesh<ExactMesh, EK> inside_check(tool);
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return inside_check(p) != CGAL::ON_BOUNDED_SIDE;
                }),
            points.end()
        );
    }

    /**
     * cut_segments_by_mesh: 1D Segment clipping by 3D Volume OR 2D Surface.
     * Keeps portions of segments that are OUTSIDE the tool.
     */
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

                bool is_on_surface = tree.do_intersect(mid);
                bool is_inside = false;
                if (is_closed) {
                    is_inside = (inside_check->operator()(mid) == CGAL::ON_BOUNDED_SIDE) || is_on_surface;
                } else {
                    is_inside = is_on_surface;
                }

                if (!is_inside) {
                    result.push_back({split_pts[i], split_pts[i+1]});
                }
            }
        }
        segments = std::move(result);
    }

    /**
     * cut_segments_by_plane: Subtraction by half-space. Keeps POSITIVE side.
     */
    static void cut_segments_by_plane(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const EK::Plane_3& plane) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            auto inter = CGAL::intersection(s, plane);
            if (inter) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&*inter)) {
                    if (!plane.has_on_negative_side(seg.first)) result.push_back({seg.first, *p});
                    if (!plane.has_on_negative_side(seg.second)) result.push_back({*p, seg.second});
                } else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&*inter)) {
                    // Entire segment on the plane, keep it (not on negative side)
                    result.push_back(seg);
                }
            } else {
                if (!plane.has_on_negative_side(seg.first)) result.push_back(seg);
            }
        }
        segments = std::move(result);
    }

    /**
     * clip_segments_by_plane: Intersection with half-space. Keeps NEGATIVE side.
     */
    static void clip_segments_by_plane(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const EK::Plane_3& plane) {
        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;
        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            auto inter = CGAL::intersection(s, plane);
            if (inter) {
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&*inter)) {
                    if (plane.has_on_negative_side(seg.first)) result.push_back({seg.first, *p});
                    if (plane.has_on_negative_side(seg.second)) result.push_back({*p, seg.second});
                } else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&*inter)) {
                    result.push_back(seg);
                }
            } else {
                if (plane.has_on_negative_side(seg.first)) result.push_back(seg);
            }
        }
        segments = std::move(result);
    }

    /**
     * clip_segments_by_mesh: Keeps only portions of segments INSIDE the tool.
     */
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

                bool is_on_surface = tree.do_intersect(mid);
                bool is_inside = false;
                if (is_closed) {
                    is_inside = (inside_check->operator()(mid) == CGAL::ON_BOUNDED_SIDE) || is_on_surface;
                } else {
                    is_inside = is_on_surface;
                }

                if (is_inside) {
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

    static ExactMesh geometry_to_mesh(const Geometry& geo) {
        std::vector<EK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));

        // CRITICAL: Prefer triangles if they exist to avoid overlapping quads/triangles
        if (!geo.triangles.empty()) {
            for (const auto& t : geo.triangles) {
                faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
            }
        } else {
            for (const auto& f : geo.faces) {
                if (f.loops.empty()) continue;
                std::vector<std::size_t> face;
                for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
                faces.push_back(face);
            }
        }

        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);

        ExactMesh mesh;
        std::vector<ExactMesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f_indices : faces) {
            std::vector<ExactMesh::Vertex_index> face_vs;
            for (auto idx : f_indices) face_vs.push_back(v_indices[idx]);
            mesh.add_face(face_vs);
        }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry(const ExactMesh& mesh) {
        Geometry geo;
        std::map<ExactMesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) {
            v_map[v] = (int)geo.vertices.size();
            auto p = mesh.point(v);
            geo.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for (auto f : mesh.faces()) {
            std::vector<int> loop;
            for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
                loop.push_back(v_map[v]);
            }
            assert(loop.size() == 3 && "ExactMesh face must be a triangle");
            geo.triangles.push_back({loop[0], loop[1], loop[2]});
        }
        return geo;
    }

    static InexactMesh geometry_to_mesh_ik(const Geometry& geo) {
        std::vector<IK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(IK::Point_3(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)));
        for (const auto& f : geo.faces) {
            if (f.loops.empty()) continue;
            std::vector<std::size_t> face;
            for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
            faces.push_back(face);
        }
        for (const auto& t : geo.triangles) {
            faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
        }
        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
        InexactMesh mesh;
        std::vector<InexactMesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f : faces) {
            std::vector<InexactMesh::Vertex_index> face_vs;
            for (auto idx : f) face_vs.push_back(v_indices[idx]);
            mesh.add_face(face_vs);
        }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry_ik(const InexactMesh& mesh) {
        Geometry geo;
        std::map<InexactMesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) {
            v_map[v] = (int)geo.vertices.size();
            auto p = mesh.point(v);
            geo.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for (auto f : mesh.faces()) {
            std::vector<int> loop;
            for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
                loop.push_back(v_map[v]);
            }
            assert(loop.size() == 3 && "InexactMesh face must be a triangle");
            geo.triangles.push_back({loop[0], loop[1], loop[2]});
        }
        return geo;
    }

    static void transform_mesh(ExactMesh& mesh, const Matrix& tf) {
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

    struct ToolNode {
        Geometry geo;
        Matrix world_tf;
        std::string type;
    };

    static void collect_tool_geometry(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<ToolNode>& tool_nodes) {
        Matrix current_tf = parent_tf * s.tf;
        std::string type = s.tags.value("type", "");
        
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            tool_nodes.push_back({geo, current_tf, type});
        } else if (type == "plane") {
            tool_nodes.push_back({Geometry(), current_tf, type});
        }

        for (const auto& child : s.components) {
            collect_tool_geometry(vfs, child, current_tf, tool_nodes);
        }
    }

    static void recursive_subtract(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes, bool open) {
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_only_points = target_geo.faces.empty() && target_geo.triangles.empty() && target_geo.segments.empty() && !target_geo.vertices.empty();

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();

                if (is_target_flat) {
                    // Try PWH-to-PWH coplanar path first
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();

                    General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, project_tf, subject_set);
                    
                    bool used_pwh_path = false;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo;
                        Matrix tool_rel_tf = subject_world_inv * tool.world_tf;
                        local_tool_geo.apply_tf(tool_rel_tf);

                        if (local_tool_geo.is_coplanar_with(target_plane)) {
                            General_polygon_set_2 tool_set;
                            add_geometry_to_gps(local_tool_geo, project_tf, tool_set);
                            cut_gps_by_gps(subject_set, tool_set);
                            used_pwh_path = true;
                        } else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                            used_pwh_path = false; break;
                        }
                    }

                    if (used_pwh_path) {
                        target_geo = gps_to_geometry(subject_set);
                        target_geo.apply_tf(rehydrate_tf);
                    } else {
                        is_target_flat = false; // Fall through to 3D
                    }
                }

                if (!is_target_flat) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        if (tool.type == "plane") {
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                            cut_mesh_by_plane(target_mesh, local_plane);
                        } else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                            ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            transform_mesh(tool_mesh, rel_tf);
                            cut_mesh_by_mesh(target_mesh, tool_mesh);
                        }
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            } else if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) {
                    local_segments.push_back({
                        EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z),
                        EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)
                    });
                }

                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                        ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        transform_mesh(tool_mesh, rel_tf);
                        cut_segments_by_mesh(local_segments, tool_mesh, tool.type == "closed");
                    } else if (tool.type == "plane") {
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                        cut_segments_by_plane(local_segments, local_plane);
                    }
                }

                Geometry res;
                for (const auto& seg : local_segments) {
                    int v1 = (int)res.vertices.size();
                    res.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()});
                    int v2 = (int)res.vertices.size();
                    res.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()});
                    res.segments.push_back({v1, v2});
                }
                target_geo = res;
            } else if (has_only_points) {
                std::vector<EK::Point_3> pts;
                for (const auto& v : target_geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));

                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                        ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        transform_mesh(tool_mesh, rel_tf);
                        cut_points_by_mesh(pts, tool_mesh);
                    } else if (tool.type == "plane") {
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                        cut_points_by_plane(pts, local_plane);
                    }
                }

                Geometry res;
                for (const auto& p : pts) res.vertices.push_back({p.x(), p.y(), p.z()});
                target_geo = res;
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_subtract(vfs, child, subject_world_tf, tool_nodes, open);
        }
    }

    static void recursive_union(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();

                if (is_target_flat) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();

                    General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, project_tf, subject_set);
                    
                    bool used_pwh_path = true;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo;
                        Matrix tool_rel_tf = subject_world_inv * tool.world_tf;
                        local_tool_geo.apply_tf(tool_rel_tf);

                        if (local_tool_geo.is_coplanar_with(target_plane)) {
                            General_polygon_set_2 tool_set;
                            add_geometry_to_gps(local_tool_geo, project_tf, tool_set);
                            join_gps_by_gps(subject_set, tool_set);
                        } else {
                            used_pwh_path = false; break;
                        }
                    }

                    if (used_pwh_path) {
                        target_geo = gps_to_geometry(subject_set);
                        target_geo.apply_tf(rehydrate_tf);
                    } else {
                        is_target_flat = false; // Fall through to 3D
                    }
                }

                if (!is_target_flat) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                            ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            transform_mesh(tool_mesh, rel_tf);
                            
                            if (CGAL::is_closed(target_mesh) && CGAL::is_closed(tool_mesh)) {
                                join_mesh_by_mesh(target_mesh, tool_mesh);
                            }
                        }
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_union(vfs, child, subject_world_tf, tool_nodes);
        }
    }

    static void recursive_intersect(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty() || !target_geo.triangles.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_only_points = target_geo.faces.empty() && target_geo.triangles.empty() && target_geo.segments.empty() && !target_geo.vertices.empty();

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();

                if (is_target_flat) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();

                    General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, project_tf, subject_set);
                    
                    bool used_pwh_path = true;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo;
                        Matrix tool_rel_tf = subject_world_inv * tool.world_tf;
                        local_tool_geo.apply_tf(tool_rel_tf);

                        if (local_tool_geo.is_coplanar_with(target_plane)) {
                            General_polygon_set_2 tool_set;
                            add_geometry_to_gps(local_tool_geo, project_tf, tool_set);
                            clip_gps_by_gps(subject_set, tool_set);
                        } else {
                            used_pwh_path = false; break;
                        }
                    }

                    if (used_pwh_path) {
                        target_geo = gps_to_geometry(subject_set);
                        target_geo.apply_tf(rehydrate_tf);
                    } else {
                        is_target_flat = false; // Fall through to 3D
                    }
                }

                if (!is_target_flat) {
                    ExactMesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        if (tool.type == "plane") {
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                            clip_mesh_by_plane(target_mesh, local_plane);
                        } else if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                            ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            transform_mesh(tool_mesh, rel_tf);
                            
                            if (CGAL::is_closed(target_mesh) && CGAL::is_closed(tool_mesh)) {
                                clip_mesh_by_mesh(target_mesh, tool_mesh);
                            }
                        }
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            } else if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) {
                    local_segments.push_back({
                        EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z),
                        EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)
                    });
                }

                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                        ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        transform_mesh(tool_mesh, rel_tf);
                        clip_segments_by_mesh(local_segments, tool_mesh, tool.type == "closed");
                    } else if (tool.type == "plane") {
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                        clip_segments_by_plane(local_segments, local_plane);
                    }
                }

                Geometry res;
                for (const auto& seg : local_segments) {
                    int v1 = (int)res.vertices.size();
                    res.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()});
                    int v2 = (int)res.vertices.size();
                    res.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()});
                    res.segments.push_back({v1, v2});
                }
                target_geo = res;
            } else if (has_only_points) {
                std::vector<EK::Point_3> pts;
                for (const auto& v : target_geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));

                for (const auto& tool : tool_nodes) {
                    if (tool.type == "closed" || tool.type == "open" || tool.type == "surface") {
                        ExactMesh tool_mesh = geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        transform_mesh(tool_mesh, rel_tf);
                        clip_points_by_mesh(pts, tool_mesh);
                    } else if (tool.type == "plane") {
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                        clip_points_by_plane(pts, local_plane);
                    }
                }

                Geometry res;
                for (const auto& p : pts) res.vertices.push_back({p.x(), p.y(), p.z()});
                target_geo = res;
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_intersect(vfs, child, subject_world_tf, tool_nodes);
        }
    }

    static void deep_disjoint(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf) {
        Matrix world_tf = parent_tf * s.tf;
        
        // 1. Process this level
        
        // Step A: Primary geometry cut by all components
        if (s.geometry.has_value() && !s.components.empty()) {
            std::vector<ToolNode> tool_nodes;
            for (const auto& child : s.components) {
                collect_tool_geometry(vfs, child, world_tf, tool_nodes);
            }
            recursive_subtract(vfs, s, parent_tf, tool_nodes, false);
        }

        // Step B: Each component cut by subsequent sibling components
        for (size_t i = 0; i < s.components.size(); ++i) {
            if (i + 1 < s.components.size()) {
                std::vector<ToolNode> tool_nodes;
                for (size_t j = i + 1; j < s.components.size(); ++j) {
                    collect_tool_geometry(vfs, s.components[j], world_tf, tool_nodes);
                }
                recursive_subtract(vfs, s.components[i], world_tf, tool_nodes, false);
            }
        }

        // 2. Recurse into children
        for (auto& child : s.components) {
            deep_disjoint(vfs, child, world_tf);
        }
    }
};

} // namespace boolean
} // namespace geo
} // namespace jotcad
