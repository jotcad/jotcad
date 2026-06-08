#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "../render/triangulation.h"
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DecalOp : P {
    static constexpr const char* path = "jot/decal";

    typedef boolean::ExactMesh ExactMesh;
    typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
    typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
    typedef CGAL::Polygon_2<EK> Polygon_2;
    typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

    struct Point3 {
        EK::Point_3 p;
        Point3(const EK::Point_3& pt) : p(pt) {}
        Point3(EK::FT x, EK::FT y, EK::FT z) : p(x, y, z) {}
    };

    // 3D Sutherland-Hodgman plane clipper
    static std::vector<EK::Point_3> clip_polygon_by_plane(const std::vector<EK::Point_3>& poly, double A, double B, double D) {
        std::vector<EK::Point_3> result;
        if (poly.empty()) return result;
        
        auto is_inside = [&](const EK::Point_3& p) {
            return (EK::FT(A) * p.x() + EK::FT(B) * p.y() + EK::FT(D)) >= 0;
        };
        
        for (size_t i = 0; i < poly.size(); ++i) {
            const auto& p_curr = poly[i];
            const auto& p_next = poly[(i + 1) % poly.size()];
            
            bool curr_in = is_inside(p_curr);
            bool next_in = is_inside(p_next);
            
            if (curr_in) {
                result.push_back(p_curr);
            }
            
            if (curr_in != next_in) {
                EK::FT val_curr = EK::FT(A) * p_curr.x() + EK::FT(B) * p_curr.y() + EK::FT(D);
                EK::FT val_next = EK::FT(A) * p_next.x() + EK::FT(B) * p_next.y() + EK::FT(D);
                EK::FT t = val_curr / (val_curr - val_next);
                
                EK::Point_3 p_inter(
                    p_curr.x() + t * (p_next.x() - p_curr.x()),
                    p_curr.y() + t * (p_next.y() - p_curr.y()),
                    p_curr.z() + t * (p_next.z() - p_curr.z())
                );
                result.push_back(p_inter);
            }
        }
        return result;
    }

    // Clip 3D polygon against a 2D convex polygon boundary (such as a face triangle)
    static std::vector<EK::Point_3> clip_polygon_by_boundary(std::vector<EK::Point_3> poly, const std::array<EK::Point_2, 3>& boundary) {
        for (int i = 0; i < 3; ++i) {
            const auto& p1 = boundary[i];
            const auto& p2 = boundary[(i + 1) % 3];
            
            double dx = CGAL::to_double(p2.x() - p1.x());
            double dy = CGAL::to_double(p2.y() - p1.y());
            
            double A = -dy;
            double B = dx;
            double D = -(A * CGAL::to_double(p1.x()) + B * CGAL::to_double(p1.y()));
            
            poly = clip_polygon_by_plane(poly, A, B, D);
            if (poly.empty()) break;
        }
        return poly;
    }

    static bool is_on_triangle_boundary(const EK::Point_2& p1, const EK::Point_2& p2, const std::array<EK::Point_2, 3>& tri) {
        auto on_segment = [](const EK::Point_2& p, const EK::Point_2& s1, const EK::Point_2& s2) {
            if (!CGAL::collinear(s1, p, s2)) return false;
            auto min_x = std::min(s1.x(), s2.x());
            auto max_x = std::max(s1.x(), s2.x());
            auto min_y = std::min(s1.y(), s2.y());
            auto max_y = std::max(s1.y(), s2.y());
            return p.x() >= min_x && p.x() <= max_x && p.y() >= min_y && p.y() <= max_y;
        };
        
        for (int i = 0; i < 3; ++i) {
            const auto& s1 = tri[i];
            const auto& s2 = tri[(i + 1) % 3];
            if (on_segment(p1, s1, s2) && on_segment(p2, s1, s2)) {
                return true;
            }
        }
        return false;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& pattern, const Shape& relief, std::string seam = "skirting", double fade_radius = 1.0) {
        std::vector<boolean::Engine::ToolNode> relief_nodes;
        boolean::Engine::collect_tool_geometry(vfs, relief, Matrix::identity(), relief_nodes);

        if (!in.geometry.has_value() || !pattern.geometry.has_value() || relief_nodes.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        try {
            // 1. Convert subject geometry to ExactMesh
            Geometry target_geo = vfs->read<Geometry>(in.geometry.value());
            ExactMesh target_mesh = boolean::Engine::geometry_to_mesh(target_geo);
            
            // Transform target mesh to world space (using in.tf)
            for (auto v : target_mesh.vertices()) {
                target_mesh.point(v) = in.tf.transform(target_mesh.point(v));
            }

            // 2. Collect pattern nodes
            std::vector<boolean::Engine::ToolNode> pattern_nodes;
            boolean::Engine::collect_tool_geometry(vfs, pattern, Matrix::identity(), pattern_nodes);

            // 3. Build combined relief mesh
            ExactMesh r_mesh_combined;
            for (const auto& relief_node : relief_nodes) {
                ExactMesh r_mesh = boolean::Engine::geometry_to_mesh(relief_node.geo);
                if (CGAL::is_closed(r_mesh)) {
                    throw std::runtime_error("Decal requires an open relief mesh (close = false)");
                }
                std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_new;
                for (auto v : r_mesh.vertices()) {
                    auto p = relief_node.world_tf.transform(r_mesh.point(v));
                    old_to_new[v] = r_mesh_combined.add_vertex(p);
                }
                for (auto f : r_mesh.faces()) {
                    std::vector<ExactMesh::Vertex_index> f_vs;
                    auto h = r_mesh.halfedge(f);
                    auto cur = h;
                    do {
                        f_vs.push_back(old_to_new[r_mesh.target(cur)]);
                        cur = r_mesh.next(cur);
                    } while (cur != h);
                    r_mesh_combined.add_face(f_vs);
                }
            }


            // Calculate vertex normal map for the subject mesh if needed
            std::map<ExactMesh::Vertex_index, EK::Vector_3> vertex_normals;
            if (seam == "smooth") {
                for (auto v : target_mesh.vertices()) {
                    vertex_normals[v] = CGAL::Polygon_mesh_processing::compute_vertex_normal(v, target_mesh);
                }
            }

            // 4. Create output mesh
            ExactMesh result_mesh;

            std::map<EK::Point_3, ExactMesh::Vertex_index> global_vertex_map;

            auto get_or_add_vertex = [&](const EK::Point_3& p) {
                auto it = global_vertex_map.find(p);
                if (it != global_vertex_map.end()) {
                    return it->second;
                }
                auto v_idx = result_mesh.add_vertex(p);
                global_vertex_map[p] = v_idx;
                return v_idx;
            };
            
            // Process each pattern node projection
            for (const auto& node : pattern_nodes) {
                Matrix world_to_local = node.world_tf.inverse();
                
                // Construct the 2D pattern polygon set in local space
                General_polygon_set_2 pattern_set;
                for (const auto& face : node.geo.faces) {
                    if (face.loops.empty()) continue;
                    Polygon_2 boundary;
                    for (int idx : face.loops[0]) {
                        boundary.push_back(EK::Point_2(node.geo.vertices[idx].x, node.geo.vertices[idx].y));
                    }
                    if (boundary.is_empty() || !boundary.is_simple()) continue;
                    if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();
                    
                    std::vector<Polygon_2> holes;
                    for (size_t i = 1; i < face.loops.size(); ++i) {
                        Polygon_2 h;
                        for (int idx : face.loops[i]) {
                            h.push_back(EK::Point_2(node.geo.vertices[idx].x, node.geo.vertices[idx].y));
                        }
                        if (h.is_simple()) {
                            if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                            holes.push_back(h);
                        }
                    }
                    pattern_set.join(General_polygon_set_2(Polygon_with_holes_2(boundary, holes.begin(), holes.end())));
                }

                if (pattern_set.is_empty()) continue;

                std::vector<Polygon_with_holes_2> pattern_pwhs;
                pattern_set.polygons_with_holes(std::back_inserter(pattern_pwhs));

                // Set up local copy of the relief mesh transformed into this pattern's local space
                std::vector<std::array<EK::Point_3, 3>> local_relief_triangles;
                for (auto f : r_mesh_combined.faces()) {
                    auto h = r_mesh_combined.halfedge(f);
                    auto p0 = world_to_local.transform(r_mesh_combined.point(r_mesh_combined.source(h)));
                    auto p1 = world_to_local.transform(r_mesh_combined.point(r_mesh_combined.target(h)));
                    auto p2 = world_to_local.transform(r_mesh_combined.point(r_mesh_combined.target(r_mesh_combined.next(h))));
                    local_relief_triangles.push_back({p0, p1, p2});
                }

                // Process each face of the target mesh
                std::vector<ExactMesh::Face_index> faces_to_remove;
                
                for (auto f : target_mesh.faces()) {
                    auto h = target_mesh.halfedge(f);
                    auto v0 = target_mesh.source(h);
                    auto v1 = target_mesh.target(h);
                    auto v2 = target_mesh.target(target_mesh.next(h));
                    
                    auto p0_world = target_mesh.point(v0);
                    auto p1_world = target_mesh.point(v1);
                    auto p2_world = target_mesh.point(v2);
                    
                    auto p0 = world_to_local.transform(p0_world);
                    auto p1 = world_to_local.transform(p1_world);
                    auto p2 = world_to_local.transform(p2_world);

                    // Check if face is vertical or facing away in local space
                    EK::Vector_3 normal_local = CGAL::normal(p0, p1, p2);
                    if (CGAL::to_double(normal_local.z()) <= 1e-3) {
                        // Face is parallel or facing away from projection direction, skip
                        continue;
                    }

                    // Project face to 2D
                    Polygon_2 face_poly_2d;
                    face_poly_2d.push_back(EK::Point_2(p0.x(), p0.y()));
                    face_poly_2d.push_back(EK::Point_2(p1.x(), p1.y()));
                    face_poly_2d.push_back(EK::Point_2(p2.x(), p2.y()));
                    if (face_poly_2d.is_clockwise_oriented()) face_poly_2d.reverse_orientation();

                    General_polygon_set_2 face_set(face_poly_2d);
                    General_polygon_set_2 intersect_set = face_set;
                    intersect_set.intersection(pattern_set);

                    if (intersect_set.is_empty()) continue;

                    faces_to_remove.push_back(f);

                    std::array<EK::Point_2, 3> tri_2d = {
                        face_poly_2d[0],
                        face_poly_2d[1],
                        face_poly_2d[2]
                    };

                    // Define the plane equation of the face in local space
                    EK::Plane_3 face_plane(p0, p1, p2);

                    auto get_face_height = [&](const EK::FT& u, const EK::FT& v) {
                        EK::FT a = face_plane.a();
                        EK::FT b = face_plane.b();
                        EK::FT c = face_plane.c();
                        EK::FT d = face_plane.d();
                        if (c == 0) return EK::FT(0);
                        return -(a * u + b * v + d) / c;
                    };

                    // A. Process inside region (intersect relief triangles with pattern and face in 2D)
                    std::vector<std::array<EK::Point_3, 3>> clipped_relief;
                    
                for (const auto& r_tri : local_relief_triangles) {
                    const auto& rp0 = r_tri[0];
                    const auto& rp1 = r_tri[1];
                    const auto& rp2 = r_tri[2];
                    
                    Polygon_2 r_tri_2d;
                    r_tri_2d.push_back(EK::Point_2(rp0.x(), rp0.y()));
                    r_tri_2d.push_back(EK::Point_2(rp1.x(), rp1.y()));
                    r_tri_2d.push_back(EK::Point_2(rp2.x(), rp2.y()));
                    if (r_tri_2d.is_empty() || !r_tri_2d.is_simple()) continue;
                    if (r_tri_2d.is_clockwise_oriented()) r_tri_2d.reverse_orientation();
                    
                    General_polygon_set_2 r_tri_set(r_tri_2d);
                    bool init_empty = r_tri_set.is_empty();
                    
                    General_polygon_set_2 r_tri_pat = r_tri_set;
                    r_tri_pat.intersection(pattern_set);
                    bool pat_empty = r_tri_pat.is_empty();
                    
                    r_tri_set.intersection(pattern_set);
                    r_tri_set.intersection(face_set);
                    bool final_empty = r_tri_set.is_empty();

                    
                    if (r_tri_set.is_empty()) continue;
                        
                        std::vector<Polygon_with_holes_2> intersect_pwhs;
                        r_tri_set.polygons_with_holes(std::back_inserter(intersect_pwhs));
                        
                        EK::Plane_3 r_plane(rp0, rp1, rp2);
                        auto get_relief_z = [&](const EK::FT& u, const EK::FT& v) {
                            EK::FT a = r_plane.a();
                            EK::FT b = r_plane.b();
                            EK::FT c = r_plane.c();
                            EK::FT d = r_plane.d();
                            if (c == 0) return EK::FT(0);
                            return -(a * u + b * v + d) / c;
                        };
                        
                        for (const auto& pwh : intersect_pwhs) {
                            CDT cdt;
                            std::map<CDT::Vertex_handle, EK::Point_2> v_map;
                            
                            auto add_loop = [&](const Polygon_2& poly) {
                                std::vector<CDT::Vertex_handle> vh;
                                for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                                    vh.push_back(cdt.insert(*it));
                                    v_map[vh.back()] = *it;
                                }
                                if (vh.size() >= 2) {
                                    for (size_t idx = 0; idx < vh.size(); ++idx) {
                                        cdt.insert_constraint(vh[idx], vh[(idx + 1) % vh.size()]);
                                    }
                                }
                            };

                            add_loop(pwh.outer_boundary());
                            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                                add_loop(*hit);
                            }

                            CGAL::mark_domain_in_triangulation(cdt);
                            for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
                                if (fit->info().in_domain) {
                                    auto pt0 = v_map[fit->vertex(0)];
                                    auto pt1 = v_map[fit->vertex(1)];
                                    auto pt2 = v_map[fit->vertex(2)];
                                    
                                    EK::FT z0 = get_relief_z(pt0.x(), pt0.y());
                                    EK::FT z1 = get_relief_z(pt1.x(), pt1.y());
                                    EK::FT z2 = get_relief_z(pt2.x(), pt2.y());
                                    
                                    clipped_relief.push_back({
                                        EK::Point_3(pt0.x(), pt0.y(), z0),
                                        EK::Point_3(pt1.x(), pt1.y(), z1),
                                        EK::Point_3(pt2.x(), pt2.y(), z2)
                                    });
                                }
                            }
                        }
                    }

                    // B. Process flat outside region
                    General_polygon_set_2 difference_set = face_set;
                    difference_set.difference(pattern_set);

                    // Add the inside displaced faces
                    std::map<EK::Point_2, ExactMesh::Vertex_index> outside_vertices_map;
                    std::map<ExactMesh::Vertex_index, EK::Point_2> vertex_local_2d;
                    std::vector<std::array<ExactMesh::Vertex_index, 3>> inside_faces;

                    auto is_on_tri_boundary = [&](const EK::Point_2& p1, const EK::Point_2& p2) {
                        auto on_segment = [](const EK::Point_2& p, const EK::Point_2& s1, const EK::Point_2& s2) {
                            if (!CGAL::collinear(s1, p, s2)) return false;
                            auto min_x = std::min(s1.x(), s2.x());
                            auto max_x = std::max(s1.x(), s2.x());
                            auto min_y = std::min(s1.y(), s2.y());
                            auto max_y = std::max(s1.y(), s2.y());
                            return p.x() >= min_x && p.x() <= max_x && p.y() >= min_y && p.y() <= max_y;
                        };
                        for (int i = 0; i < 3; ++i) {
                            const auto& s1 = tri_2d[i];
                            const auto& s2 = tri_2d[(i + 1) % 3];
                            if (on_segment(p1, s1, s2) && on_segment(p2, s1, s2)) {
                                return true;
                            }
                        }
                        return false;
                    };

                    auto add_outside_vertex = [&](const EK::Point_2& pt) {
                        auto it = outside_vertices_map.find(pt);
                        if (it != outside_vertices_map.end()) return it->second;
                        
                        EK::FT face_z = get_face_height(pt.x(), pt.y());
                        EK::Point_3 p_world = node.world_tf.transform(EK::Point_3(pt.x(), pt.y(), face_z));
                        
                        auto v_idx = get_or_add_vertex(p_world);
                        outside_vertices_map[pt] = v_idx;
                        vertex_local_2d[v_idx] = pt;
                        return v_idx;
                    };

                    auto add_inside_vertex = [&](const EK::Point_3& pt_local) {
                        EK::Point_2 pt(pt_local.x(), pt_local.y());
                        EK::FT z_relief = pt_local.z();

                        
                        if (z_relief == 0) {
                            return add_outside_vertex(pt);
                        }
                        
                        EK::FT face_z = get_face_height(pt.x(), pt.y());
                        EK::FT z_final = face_z + z_relief;

                        // Apply boundary attenuation if needed
                        if (seam == "attenuate" || seam == "smooth") {
                            // Find distance to pattern boundary
                            EK::FT min_d_pattern_sq = -1;
                            for (const auto& pwh : pattern_pwhs) {
                                const auto& outer = pwh.outer_boundary();
                                for (size_t idx = 0; idx < outer.size(); ++idx) {
                                    EK::FT d_sq = CGAL::squared_distance(
                                        pt,
                                        EK::Segment_2(outer[idx], outer[(idx+1)%outer.size()])
                                    );
                                    if (min_d_pattern_sq < 0 || d_sq < min_d_pattern_sq) {
                                        min_d_pattern_sq = d_sq;
                                    }
                                }
                                for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                                    for (size_t idx = 0; idx < hit->size(); ++idx) {
                                        EK::FT d_sq = CGAL::squared_distance(
                                            pt,
                                            EK::Segment_2((*hit)[idx], (*hit)[(idx+1)%hit->size()])
                                        );
                                        if (min_d_pattern_sq < 0 || d_sq < min_d_pattern_sq) {
                                            min_d_pattern_sq = d_sq;
                                        }
                                    }
                                }
                            }
                            double d = std::sqrt(CGAL::to_double(min_d_pattern_sq));
                            double t = std::clamp(d / fade_radius, 0.0, 1.0);
                            double weight = t;
                            if (seam == "smooth") {
                                weight = t * t * (3.0 - 2.0 * t);
                            }
                            z_final = face_z + z_relief * EK::FT(weight);
                        }

                        EK::Point_3 p_world = node.world_tf.transform(EK::Point_3(pt.x(), pt.y(), z_final));
                        
                        auto v_idx = get_or_add_vertex(p_world);
                        vertex_local_2d[v_idx] = pt;
                        return v_idx;
                    };

                    for (const auto& tri : clipped_relief) {
                        auto vi0 = add_inside_vertex(tri[0]);
                        auto vi1 = add_inside_vertex(tri[1]);
                        auto vi2 = add_inside_vertex(tri[2]);
                        if (vi0 != vi1 && vi1 != vi2 && vi0 != vi2) {
                            result_mesh.add_face(vi0, vi1, vi2);
                            inside_faces.push_back({vi0, vi1, vi2});
                        }
                    }

                    // Find all boundary edges of the inside region and identify seam edges/points
                    std::map<std::pair<ExactMesh::Vertex_index, ExactMesh::Vertex_index>, int> edge_counts;
                    for (const auto& f : inside_faces) {
                        for (int i = 0; i < 3; ++i) {
                            auto v1 = f[i];
                            auto v2 = f[(i+1)%3];
                            auto edge = std::make_pair(std::min(v1, v2), std::max(v1, v2));
                            edge_counts[edge]++;
                        }
                    }

                    std::vector<std::pair<ExactMesh::Vertex_index, ExactMesh::Vertex_index>> seam_edges;
                    std::set<EK::Point_2> unique_seam_pts;
                    std::vector<EK::Point_2> relief_boundary_pts;

                    for (const auto& [edge, count] : edge_counts) {
                        if (count == 1) {
                            auto v1 = edge.first;
                            auto v2 = edge.second;
                            const auto& pt1 = vertex_local_2d[v1];
                            const auto& pt2 = vertex_local_2d[v2];
                            
                            if (!is_on_tri_boundary(pt1, pt2)) {
                                seam_edges.push_back(edge);
                                
                                if (unique_seam_pts.insert(pt1).second) {
                                    relief_boundary_pts.push_back(pt1);
                                }
                                if (unique_seam_pts.insert(pt2).second) {
                                    relief_boundary_pts.push_back(pt2);
                                }
                            }
                        }
                    }

                    std::vector<Polygon_with_holes_2> outside_pwhs;
                    difference_set.polygons_with_holes(std::back_inserter(outside_pwhs));

                    for (const auto& pwh : outside_pwhs) {
                        CDT cdt;
                        std::map<CDT::Vertex_handle, EK::Point_2> v_map;
                        
                        auto add_loop = [&](const Polygon_2& poly) {
                            std::vector<CDT::Vertex_handle> vh;
                            for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                                vh.push_back(cdt.insert(*it));
                                v_map[vh.back()] = *it;
                            }
                            if (vh.size() >= 2) {
                                for (size_t idx = 0; idx < vh.size(); ++idx) {
                                    cdt.insert_constraint(vh[idx], vh[(idx + 1) % vh.size()]);
                                }
                            }
                        };

                        add_loop(pwh.outer_boundary());
                        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                            add_loop(*hit);
                        }

                        // Insert relief boundary vertices lying on the pattern boundary to align the triangulations
                        for (const auto& pt : relief_boundary_pts) {
                            // Insert only if the point lies inside or on the boundary of the pwh
                            if (pwh.outer_boundary().has_on_bounded_side(pt) || 
                                pwh.outer_boundary().has_on_boundary(pt)) {
                                bool in_hole = false;
                                for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                                    if (hit->has_on_bounded_side(pt)) {
                                        in_hole = true;
                                        break;
                                    }
                                }
                                if (!in_hole) {
                                    auto vh = cdt.insert(pt);
                                    v_map[vh] = pt;
                                }
                            }
                        }

                        CGAL::mark_domain_in_triangulation(cdt);
                        for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
                            if (fit->info().in_domain) {
                                auto pt0 = v_map[fit->vertex(0)];
                                auto pt1 = v_map[fit->vertex(1)];
                                auto pt2 = v_map[fit->vertex(2)];
                                
                                auto vo0 = add_outside_vertex(pt0);
                                auto vo1 = add_outside_vertex(pt1);
                                auto vo2 = add_outside_vertex(pt2);
                                if (vo0 != vo1 && vo1 != vo2 && vo0 != vo2) {
                                    result_mesh.add_face(vo0, vo1, vo2);
                                }
                            }
                        }
                    }

                    // C. Process vertical skirting (always needed for interior height discontinuities)
                    if (true) {
                        auto add_oriented_face = [&](ExactMesh::Vertex_index a, ExactMesh::Vertex_index b, ExactMesh::Vertex_index c) {
                            auto has_face = [&](ExactMesh::Vertex_index u, ExactMesh::Vertex_index v) {
                                ExactMesh::Halfedge_index h = ExactMesh::null_halfedge();
                                ExactMesh::Halfedge_index h_start = result_mesh.halfedge(v);
                                if (h_start != ExactMesh::null_halfedge()) {
                                    for (auto he : result_mesh.halfedges_around_target(h_start)) {
                                        if (result_mesh.source(he) == u) {
                                            h = he;
                                            break;
                                        }
                                    }
                                }
                                return h != ExactMesh::null_halfedge() && result_mesh.face(h) != ExactMesh::null_face();
                            };
                            
                            bool normal_ok = !has_face(a, b) && !has_face(b, c) && !has_face(c, a);
                            bool reverse_ok = !has_face(a, c) && !has_face(c, b) && !has_face(b, a);
                            
                            if (normal_ok && !reverse_ok) {
                                return result_mesh.add_face(a, b, c);
                            } else if (reverse_ok && !normal_ok) {
                                return result_mesh.add_face(a, c, b);
                            } else if (normal_ok && reverse_ok) {
                                return result_mesh.add_face(a, b, c);
                            } else {
                                return ExactMesh::null_face();
                            }
                        };

                        for (const auto& edge : seam_edges) {
                            auto v1 = edge.first;
                            auto v2 = edge.second;
                            const auto& pt1 = vertex_local_2d[v1];
                            const auto& pt2 = vertex_local_2d[v2];
                            
                            auto v_outside1 = add_outside_vertex(pt1);
                            auto v_outside2 = add_outside_vertex(pt2);
                            
                            if (v1 == v_outside1 && v2 == v_outside2) {
                                continue;
                            }
                            
                            // Determine the correct orientation of the skirting faces based on the inside triangulation
                            bool forward = false;
                            for (const auto& f : inside_faces) {
                                if ((f[0] == v1 && f[1] == v2) || (f[1] == v1 && f[2] == v2) || (f[2] == v1 && f[0] == v2)) {
                                     forward = true;
                                     break;
                                }
                            }
                            
                            bool has_f1 = true;
                            bool has_f2 = true;
                            if (v1 == v_outside1) {
                                if (forward) has_f2 = false;
                                else has_f1 = false;
                            }
                            if (v2 == v_outside2) {
                                if (forward) has_f1 = false;
                                else has_f2 = false;
                            }
                            
                            ExactMesh::Face_index f1 = ExactMesh::null_face();
                            ExactMesh::Face_index f2 = ExactMesh::null_face();
                            
                            if (has_f1) {
                                f1 = forward ? add_oriented_face(v_outside1, v_outside2, v2)
                                             : add_oriented_face(v_outside2, v_outside1, v1);
                            }
                            if (has_f2) {
                                f2 = forward ? add_oriented_face(v_outside1, v2, v1)
                                             : add_oriented_face(v_outside2, v1, v2);
                            }
                            
                            if ((has_f1 && f1 == ExactMesh::null_face()) || (has_f2 && f2 == ExactMesh::null_face())) {
                                std::cerr << "[DecalOp] Warning: Skirting face creation failed!" << std::endl;
                                std::cerr << "  v1=" << v1 << " v2=" << v2 << " v_outside1=" << v_outside1 << " v_outside2=" << v_outside2 << std::endl;
                                std::cerr << "  v1 coords: " << result_mesh.point(v1) << " v2 coords: " << result_mesh.point(v2) << std::endl;
                                std::cerr << "  vo1 coords: " << result_mesh.point(v_outside1) << " vo2 coords: " << result_mesh.point(v_outside2) << std::endl;
                                std::cerr << "  has_f1=" << has_f1 << " has_f2=" << has_f2 << " forward=" << forward << std::endl;
                                auto check_edge = [&](ExactMesh::Vertex_index u, ExactMesh::Vertex_index v, const std::string& name) {
                                    ExactMesh::Halfedge_index h = ExactMesh::null_halfedge();
                                    ExactMesh::Halfedge_index h_start = result_mesh.halfedge(v);
                                    if (h_start != ExactMesh::null_halfedge()) {
                                        for (auto he : result_mesh.halfedges_around_target(h_start)) {
                                            if (result_mesh.source(he) == u) { h = he; break; }
                                        }
                                    }
                                    if (h != ExactMesh::null_halfedge()) {
                                        std::cerr << "    Edge " << name << " exists, face=" << result_mesh.face(h) << std::endl;
                                    }
                                };
                                check_edge(v_outside1, v_outside2, "vo1 -> vo2");
                                check_edge(v_outside2, v_outside1, "vo2 -> vo1");
                                check_edge(v1, v2, "v1 -> v2");
                                check_edge(v2, v1, "v2 -> v1");
                            }
                        }
                    }
                }

                // Add all unaffected target mesh faces to result_mesh
                std::set<ExactMesh::Face_index> remove_set(faces_to_remove.begin(), faces_to_remove.end());
                std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_new_unaffected;
                
                auto add_unaffected_vertex = [&](ExactMesh::Vertex_index v) {
                    auto it = old_to_new_unaffected.find(v);
                    if (it != old_to_new_unaffected.end()) return it->second;
                    auto p = target_mesh.point(v);
                    auto v_idx = get_or_add_vertex(p);
                    old_to_new_unaffected[v] = v_idx;
                    return v_idx;
                };

                for (auto f : target_mesh.faces()) {
                    if (remove_set.count(f) == 0) {
                        std::vector<ExactMesh::Vertex_index> f_vs;
                        auto h = target_mesh.halfedge(f);
                        auto cur = h;
                        do {
                            f_vs.push_back(add_unaffected_vertex(target_mesh.target(cur)));
                            cur = target_mesh.next(cur);
                        } while (cur != h);
                        result_mesh.add_face(f_vs);
                    }
                }
            }

            // 5. Clean up mesh and transform back to local space using in.tf.inverse()
            
            CGAL::Polygon_mesh_processing::remove_isolated_vertices(result_mesh);
            
            Matrix world_to_local = in.tf.inverse();
            for (auto v : result_mesh.vertices()) {
                result_mesh.point(v) = world_to_local.transform(result_mesh.point(v));
            }

            Shape out = in;
            out.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry(result_mesh));
            vfs->write(fulfilling.with_output("$out"), out);

        } catch (const std::exception& e) {
            std::cerr << "[DecalOp] Error: " << e.what() << std::endl;
            throw;
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "pattern", "relief", "seam", "fade_radius"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/decal"},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}},
                {"pattern", {{"type", "jot:shape"}}},
                {"relief", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "seam"}, {"type", "jot:string"}, {"default", "skirting"}},
                {{"name", "fade_radius"}, {"type", "jot:number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void decal_init(fs::VFSNode* vfs) {
    Processor::register_op<DecalOp<>, Shape, Shape, Shape, std::string, double>(vfs, "jot/decal");
}

} // namespace geo
} // namespace jotcad
