#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Cartesian_converter.h>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct EmbossOp : P {
    static constexpr const char* path = "jot/emboss";

    typedef boolean::ExactMesh ExactMesh;
    typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
    typedef CGAL::AABB_traits<EK, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& pattern, const Shape& relief) {
        if (!in.geometry.has_value() || !pattern.geometry.has_value() || !relief.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        try {
            // 1. Read target and relief geometry, convert to ExactMesh
            Geometry target_geo = vfs->read<Geometry>(in.geometry.value());
            ExactMesh target_mesh = boolean::Engine::geometry_to_mesh(target_geo);
            
            // Transform target mesh to world space (using in.tf)
            for (auto v : target_mesh.vertices()) {
                target_mesh.point(v) = in.tf.transform(target_mesh.point(v));
            }

            Geometry relief_geo = vfs->read<Geometry>(relief.geometry.value());

            // 2. Collect pattern nodes and build combined column cutter mesh
            std::vector<boolean::Engine::ToolNode> pattern_nodes;
            boolean::Engine::collect_tool_geometry(vfs, pattern, Matrix::identity(), pattern_nodes);

            struct NodeRelief {
                std::unique_ptr<ExactMesh> mesh;
                std::unique_ptr<Tree> tree;
            };
            std::vector<NodeRelief> node_reliefs;

            for (const auto& node : pattern_nodes) {
                auto r_mesh = std::make_unique<ExactMesh>(boolean::Engine::geometry_to_mesh(relief_geo));
                
                // Transform relief mesh from its local space to the pattern node's local space
                Matrix T_relief_to_pattern = node.world_tf.inverse() * relief.tf;
                for (auto v : r_mesh->vertices()) {
                    r_mesh->point(v) = T_relief_to_pattern.transform(r_mesh->point(v));
                }

                auto tree_ptr = std::make_unique<Tree>(faces(*r_mesh).first, faces(*r_mesh).second, *r_mesh);
                tree_ptr->build();
                tree_ptr->accelerate_distance_queries();
                
                node_reliefs.push_back({std::move(r_mesh), std::move(tree_ptr)});
            }

            ExactMesh cutter_mesh;

            for (const auto& node : pattern_nodes) {
                // Construct open pattern mesh
                ExactMesh pattern_mesh = boolean::Engine::geometry_to_mesh(node.geo);

                // Build top/bottom and side faces of the prism in local space
                std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_top;
                std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_bot;

                for (auto v : pattern_mesh.vertices()) {
                    auto p = pattern_mesh.point(v);
                    double px = CGAL::to_double(p.x());
                    double py = CGAL::to_double(p.y());
                    // Extrude column in local space, then transform to world space
                    EK::Point_3 top_p = node.world_tf.transform(EK::Point_3(px, py, 10000.0));
                    EK::Point_3 bot_p = node.world_tf.transform(EK::Point_3(px, py, -10000.0));
                    
                    old_to_top[v] = cutter_mesh.add_vertex(top_p);
                    old_to_bot[v] = cutter_mesh.add_vertex(bot_p);
                }

                // Add top faces
                for (auto f : pattern_mesh.faces()) {
                    std::vector<ExactMesh::Vertex_index> f_vs;
                    auto h = pattern_mesh.halfedge(f);
                    auto cur = h;
                    do {
                        f_vs.push_back(old_to_top[pattern_mesh.target(cur)]);
                        cur = pattern_mesh.next(cur);
                    } while (cur != h);
                    cutter_mesh.add_face(f_vs);
                }

                // Add bottom faces (reversed)
                for (auto f : pattern_mesh.faces()) {
                    std::vector<ExactMesh::Vertex_index> f_vs;
                    auto h = pattern_mesh.halfedge(f);
                    auto cur = h;
                    do {
                        f_vs.push_back(old_to_bot[pattern_mesh.target(cur)]);
                        cur = pattern_mesh.next(cur);
                    } while (cur != h);
                    std::reverse(f_vs.begin(), f_vs.end());
                    cutter_mesh.add_face(f_vs);
                }

                // Add side walls
                for (auto h : pattern_mesh.halfedges()) {
                    if (pattern_mesh.is_border(h)) {
                        auto vs = pattern_mesh.source(h);
                        auto vt = pattern_mesh.target(h);
                        auto ts = old_to_top[vs];
                        auto tt = old_to_top[vt];
                        auto bs = old_to_bot[vs];
                        auto bt = old_to_bot[vt];
                        cutter_mesh.add_face(ts, tt, bt);
                        cutter_mesh.add_face(ts, bt, bs);
                    }
                }
            }

            // 3. Split target_mesh with cutter_mesh
            CGAL::Polygon_mesh_processing::split(target_mesh, cutter_mesh);

            // 4. Build Side_of_triangle_mesh to check inside/outside
            CGAL::Side_of_triangle_mesh<ExactMesh, EK> inside_check(cutter_mesh);

            // 5. Classify target_mesh faces
            std::vector<ExactMesh::Face_index> inside_faces;
            std::vector<ExactMesh::Face_index> outside_faces;
            std::set<ExactMesh::Face_index> inside_set;

            for (auto f : target_mesh.faces()) {
                auto h = target_mesh.halfedge(f);
                auto p0 = target_mesh.point(target_mesh.source(h));
                auto p1 = target_mesh.point(target_mesh.source(target_mesh.opposite(h)));
                auto p2 = target_mesh.point(target_mesh.target(target_mesh.next(h)));
                auto mid = CGAL::centroid(p0, p1, p2);
                if (inside_check(mid) != CGAL::ON_UNBOUNDED_SIDE) {
                    inside_faces.push_back(f);
                    inside_set.insert(f);
                } else {
                    outside_faces.push_back(f);
                }
            }

            if (inside_faces.empty()) {
                vfs->write(fulfilling.with_output("$out"), in);
                return;
            }

            // 6. Find boundary halfedges between inside and outside faces
            std::vector<ExactMesh::Halfedge_index> boundary_halfedges;
            for (auto f : inside_faces) {
                auto h = target_mesh.halfedge(f);
                auto cur = h;
                do {
                    auto opp = target_mesh.opposite(cur);
                    auto opp_f = target_mesh.face(opp);
                    if (opp_f == ExactMesh::null_face() || inside_set.find(opp_f) == inside_set.end()) {
                        boundary_halfedges.push_back(cur);
                    }
                    cur = target_mesh.next(cur);
                } while (cur != h);
            }

            // 7. Create the result mesh
            ExactMesh result_mesh;
            std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_new_outside;
            std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> old_to_new_inside;

            std::set<ExactMesh::Vertex_index> boundary_vertices;
            for (auto h : boundary_halfedges) {
                boundary_vertices.insert(target_mesh.source(h));
                boundary_vertices.insert(target_mesh.target(h));
            }

            for (auto v : target_mesh.vertices()) {
                auto p = target_mesh.point(v);
                bool is_boundary = boundary_vertices.find(v) != boundary_vertices.end();
                
                bool is_used_outside = false;
                bool is_used_inside = false;
                
                auto h = target_mesh.halfedge(v);
                if (h != ExactMesh::null_halfedge()) {
                    auto cur = h;
                    do {
                        auto f = target_mesh.face(cur);
                        if (f != ExactMesh::null_face()) {
                            if (inside_set.find(f) != inside_set.end()) is_used_inside = true;
                            else is_used_outside = true;
                        }
                        cur = target_mesh.next_around_target(cur);
                    } while (cur != h);
                }
                
                if (is_used_outside || is_boundary) {
                    old_to_new_outside[v] = result_mesh.add_vertex(p);
                }

                if (is_used_inside || is_boundary) {
                    // Compute vertex normal in target_mesh
                    EK::Vector_3 normal_ek = CGAL::Polygon_mesh_processing::compute_vertex_normal(v, target_mesh);
                    double nx = CGAL::to_double(normal_ek.x());
                    double ny = CGAL::to_double(normal_ek.y());
                    double nz = CGAL::to_double(normal_ek.z());
                    double len = std::sqrt(nx*nx + ny*ny + nz*nz);
                    if (len > 1e-9) { nx /= len; ny /= len; nz /= len; }

                    // Find closest pattern node for local direction
                    double best_dist = 1e18;
                    size_t best_node = 0;
                    for (size_t i = 0; i < pattern_nodes.size(); ++i) {
                        auto p_local = pattern_nodes[i].world_tf.inverse().transform(p);
                        double dist_to_center = std::sqrt(std::pow(CGAL::to_double(p_local.x()), 2) + std::pow(CGAL::to_double(p_local.y()), 2));
                        if (dist_to_center < best_dist) {
                            best_dist = dist_to_center;
                            best_node = i;
                        }
                    }

                    // Project point to local space of the best pattern node
                    EK::Point_3 p_local = pattern_nodes[best_node].world_tf.inverse().transform(p);

                    // Construct ray in the local space of the pattern node pointing downward along local Z axis
                    EK::Ray_3 ray(EK::Point_3(p_local.x(), p_local.y(), EK::FT(10000.0)), EK::Vector_3(0, 0, -1));

                    std::vector<typename Tree::Intersection_and_primitive_id<EK::Ray_3>::Type> intersections;
                    node_reliefs[best_node].tree->all_intersections(ray, std::back_inserter(intersections));

                    double max_z = -1e18;
                    bool hit = false;
                    for (const auto& inter : intersections) {
                        if (const EK::Point_3* pi = std::get_if<EK::Point_3>(&inter.first)) {
                            double z_val = CGAL::to_double(pi->z());
                            if (z_val > max_z) {
                                max_z = z_val;
                                hit = true;
                            }
                        }
                    }
                    double height_val = hit ? max_z : 0.0;

                    EK::Point_3 displaced_p(
                        p.x() + EK::FT(nx * height_val),
                        p.y() + EK::FT(ny * height_val),
                        p.z() + EK::FT(nz * height_val)
                    );
                    old_to_new_inside[v] = result_mesh.add_vertex(displaced_p);
                }
            }

            // 8. Add outside faces to result
            for (auto f : outside_faces) {
                std::vector<ExactMesh::Vertex_index> f_vs;
                auto h = target_mesh.halfedge(f);
                auto cur = h;
                do {
                    f_vs.push_back(old_to_new_outside[target_mesh.target(cur)]);
                    cur = target_mesh.next(cur);
                } while (cur != h);
                result_mesh.add_face(f_vs);
            }

            // 9. Add inside faces to result
            for (auto f : inside_faces) {
                std::vector<ExactMesh::Vertex_index> f_vs;
                auto h = target_mesh.halfedge(f);
                auto cur = h;
                do {
                    f_vs.push_back(old_to_new_inside[target_mesh.target(cur)]);
                    cur = target_mesh.next(cur);
                } while (cur != h);
                result_mesh.add_face(f_vs);
            }

            // 10. Add side walls connecting outside and inside boundaries
            for (auto h : boundary_halfedges) {
                auto vs = target_mesh.source(h);
                auto vt = target_mesh.target(h);
                
                auto os = old_to_new_outside[vs];
                auto ot = old_to_new_outside[vt];
                auto is = old_to_new_inside[vs];
                auto it = old_to_new_inside[vt];
                
                result_mesh.add_face(os, ot, it);
                result_mesh.add_face(os, it, is);
            }

            // 11. Convert back to local space using in.tf.inverse()
            Matrix world_to_local = in.tf.inverse();
            for (auto v : result_mesh.vertices()) {
                result_mesh.point(v) = world_to_local.transform(result_mesh.point(v));
            }

            Shape out = in;
            out.geometry = vfs->materialize<Geometry>(boolean::Engine::mesh_to_geometry(result_mesh));
            vfs->write(fulfilling.with_output("$out"), out);

        } catch (const std::exception& e) {
            std::cerr << "[EmbossOp] Error: " << e.what() << std::endl;
            vfs->write(fulfilling.with_output("$out"), in);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "pattern", "relief"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/emboss"},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}},
                {"pattern", {{"type", "jot:shape"}}},
                {"relief", {{"type", "jot:shape"}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void emboss_init(fs::VFSNode* vfs) {
    Processor::register_op<EmbossOp<>, Shape, Shape, Shape>(vfs, "jot/emboss");
}

} // namespace geo
} // namespace jotcad
