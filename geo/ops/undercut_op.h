#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "triangulation.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <memory>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct UndercutOp : P {
    static constexpr const char* path = "jot/undercut";

    typedef CGAL::Surface_mesh<IK::Point_3> Mesh;
    typedef CGAL::AABB_face_graph_triangle_primitive<Mesh> Primitive;
    typedef CGAL::AABB_traits<IK, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;

    static void collect_world_geometry_recursive(fs::VFSNode* vfs, const Shape& s, const Matrix& current_tf, Geometry& world_geo) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>(s.geometry.value());
            int offset = (int)world_geo.vertices.size();
            for (const auto& v : geo.vertices) {
                EK::Point_3 p = current_tf.transform(EK::Point_3(v.x, v.y, v.z));
                world_geo.vertices.push_back({p.x(), p.y(), p.z()});
            }
            
            if (!geo.triangles.empty()) {
                for (const auto& tri : geo.triangles) {
                    world_geo.triangles.push_back({tri[0] + offset, tri[1] + offset, tri[2] + offset});
                }
            } else if (!geo.faces.empty()) {
                std::vector<Vec3> pts;
                for (const auto& v : geo.vertices) {
                    pts.push_back(Vec3{CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
                }
                for (const auto& f : geo.faces) {
                    Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
                        world_geo.triangles.push_back({i0 + offset, i1 + offset, i2 + offset});
                    });
                }
            }
        }
        for (const auto& child : s.components) {
            collect_world_geometry_recursive(vfs, child, current_tf * child.tf, world_geo);
        }
    }

    static Shape analyze_shape_recursive(fs::VFSNode* vfs, const Shape& in, const Matrix& current_tf, double dx, double dy, double dz, double sin_alpha, double L, const Tree* tree, const Mesh* world_mesh) {
        Shape out = in;
        out.components.clear();
        
        if (in.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>(in.geometry.value());
            
            Geometry safe_geo, undercut_geo, flat_geo;
            std::map<int, int> safe_vmap, undercut_vmap, flat_vmap;
            
            auto add_triangle_to_geo = [&](Geometry& target_geo, std::map<int, int>& vertex_map, int i0, int i1, int i2) {
                auto get_new_idx = [&](int old_idx) -> int {
                    auto it = vertex_map.find(old_idx);
                    if (it != vertex_map.end()) return it->second;
                    int new_idx = (int)target_geo.vertices.size();
                    target_geo.vertices.push_back(geo.vertices[old_idx]);
                    vertex_map[old_idx] = new_idx;
                    return new_idx;
                };
                int n0 = get_new_idx(i0);
                int n1 = get_new_idx(i1);
                int n2 = get_new_idx(i2);
                target_geo.triangles.push_back({n0, n1, n2});
            };

            CGAL::Cartesian_converter<EK, IK> ek_to_ik;
            IK::Vector_3 dir_ik(dx, dy, dz);

            auto process_triangle = [&](int i0, int i1, int i2, 
                                        double p0_x, double p0_y, double p0_z,
                                        double p1_x, double p1_y, double p1_z,
                                        double p2_x, double p2_y, double p2_z) {
                EK::Point_3 w0 = current_tf.transform(EK::Point_3(p0_x, p0_y, p0_z));
                EK::Point_3 w1 = current_tf.transform(EK::Point_3(p1_x, p1_y, p1_z));
                EK::Point_3 w2 = current_tf.transform(EK::Point_3(p2_x, p2_y, p2_z));

                double centroid_x = CGAL::to_double(w0.x() + w1.x() + w2.x()) / 3.0;
                double centroid_y = CGAL::to_double(w0.y() + w1.y() + w2.y()) / 3.0;
                double centroid_z = CGAL::to_double(w0.z() + w1.z() + w2.z()) / 3.0;

                IK::Point_3 c_world_ik(centroid_x, centroid_y, centroid_z);

                double ux = CGAL::to_double(w1.x() - w0.x());
                double uy = CGAL::to_double(w1.y() - w0.y());
                double uz = CGAL::to_double(w1.z() - w0.z());

                double vx = CGAL::to_double(w2.x() - w0.x());
                double vy = CGAL::to_double(w2.y() - w0.y());
                double vz = CGAL::to_double(w2.z() - w0.z());

                double nx = uy * vz - uz * vy;
                double ny = uz * vx - ux * vz;
                double nz = ux * vy - uy * vx;

                double len = std::sqrt(nx*nx + ny*ny + nz*nz);
                if (len > 1e-12) {
                    nx /= len;
                    ny /= len;
                    nz /= len;
                }
                
                double dot = nx * dx + ny * dy + nz * dz;
                
                bool trapped = false;
                if (tree) {
                    if (std::abs(dot) > sin_alpha) {
                        IK::Vector_3 ray_dir = (dot > 0.0) ? dir_ik : -dir_ik;
                        IK::Ray_3 ray(c_world_ik, ray_dir);

                        std::vector<typename Tree::Intersection_and_primitive_id<IK::Ray_3>::Type> intersections;
                        tree->all_intersections(ray, std::back_inserter(intersections));

                        for (const auto& inter : intersections) {
                            IK::Point_3 pt;
                            if (const IK::Point_3* pi = std::get_if<IK::Point_3>(&inter.first)) {
                                pt = *pi;
                            } else if (const IK::Segment_3* ps = std::get_if<IK::Segment_3>(&inter.first)) {
                                pt = ps->source();
                            } else {
                                continue;
                            }

                            // Ignore intersections at the starting point (source triangle)
                            if (CGAL::to_double(CGAL::squared_distance(pt, c_world_ik)) > 1e-6) {
                                trapped = true;
                                break;
                            }
                        }
                    }
                }

                if (trapped) {
                    add_triangle_to_geo(flat_geo, flat_vmap, i0, i1, i2);
                } else {
                    if (dot > sin_alpha) {
                        add_triangle_to_geo(safe_geo, safe_vmap, i0, i1, i2);
                    } else if (dot < -sin_alpha) {
                        add_triangle_to_geo(undercut_geo, undercut_vmap, i0, i1, i2);
                    } else {
                        add_triangle_to_geo(flat_geo, flat_vmap, i0, i1, i2);
                    }
                }
            };

            // 1. Process explicit triangles
            if (!geo.triangles.empty()) {
                for (const auto& tri : geo.triangles) {
                    int i0 = tri[0];
                    int i1 = tri[1];
                    int i2 = tri[2];
                    
                    process_triangle(i0, i1, i2,
                                     CGAL::to_double(geo.vertices[i0].x),
                                     CGAL::to_double(geo.vertices[i0].y),
                                     CGAL::to_double(geo.vertices[i0].z),
                                     CGAL::to_double(geo.vertices[i1].x),
                                     CGAL::to_double(geo.vertices[i1].y),
                                     CGAL::to_double(geo.vertices[i1].z),
                                     CGAL::to_double(geo.vertices[i2].x),
                                     CGAL::to_double(geo.vertices[i2].y),
                                     CGAL::to_double(geo.vertices[i2].z));
                }
            } else if (!geo.faces.empty()) {
                // 2. Process general faces (by triangulating them first)
                std::vector<Vec3> pts;
                for (const auto& v : geo.vertices) {
                    pts.push_back(Vec3{CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
                }

                for (const auto& f : geo.faces) {
                    Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
                        process_triangle(i0, i1, i2,
                                         pts[i0].x, pts[i0].y, pts[i0].z,
                                         pts[i1].x, pts[i1].y, pts[i1].z,
                                         pts[i2].x, pts[i2].y, pts[i2].z);
                    });
                }
            }

            // 3. Assemble sub-mesh components
            std::vector<Shape> sub_shapes;
            
            if (!safe_geo.triangles.empty()) {
                Shape s_safe = P::make_shape(vfs, safe_geo, {{"color", "#2bee2b"}, {"name", "safe_faces"}});
                sub_shapes.push_back(s_safe);
            }
            
            if (!undercut_geo.triangles.empty()) {
                Shape s_undercut = P::make_shape(vfs, undercut_geo, {{"color", "#ee2b2b"}, {"name", "undercut_faces"}});
                sub_shapes.push_back(s_undercut);
            }
            
            if (!flat_geo.triangles.empty()) {
                Shape s_flat = P::make_shape(vfs, flat_geo, {{"color", "#eeee2b"}, {"name", "flat_faces"}});
                sub_shapes.push_back(s_flat);
            }
            
            out.geometry = std::nullopt;
            out.components = sub_shapes;
            out.add_tag("type", "group");
        }

        // Process children recursively
        for (const auto& child : in.components) {
            out.components.push_back(analyze_shape_recursive(vfs, child, current_tf * child.tf, dx, dy, dz, sin_alpha, L, tree, world_mesh));
        }
        
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double dx, double dy, double dz, double min_draft = 0.5) {
        // Normalize pull vector direction
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 1e-12) {
            dx /= len;
            dy /= len;
            dz /= len;
        } else {
            dx = 0.0; dy = 0.0; dz = 1.0; // Default: Z-up
        }

        double sin_alpha = std::sin(min_draft * M_PI / 180.0);

        // 1. Flatten/collect all geometry in world coordinates to build a global AABB tree
        Geometry world_geo;
        collect_world_geometry_recursive(vfs, in, Matrix::identity(), world_geo);
        
        double L = 100.0;
        std::unique_ptr<Tree> tree = nullptr;
        std::unique_ptr<Mesh> world_mesh = nullptr;
        if (!world_geo.triangles.empty()) {
            // Calculate a safe starting distance outside the bounding box
            double min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
            bool first = true;
            for (const auto& v : world_geo.vertices) {
                double vx = CGAL::to_double(v.x);
                double vy = CGAL::to_double(v.y);
                double vz = CGAL::to_double(v.z);
                if (first) {
                    min_x = max_x = vx;
                    min_y = max_y = vy;
                    min_z = max_z = vz;
                    first = false;
                } else {
                    min_x = std::min(min_x, vx); max_x = std::max(max_x, vx);
                    min_y = std::min(min_y, vy); max_y = std::max(max_y, vy);
                    min_z = std::min(min_z, vz); max_z = std::max(max_z, vz);
                }
            }
            double dx_box = max_x - min_x;
            double dy_box = max_y - min_y;
            double dz_box = max_z - min_z;
            double diag = std::sqrt(dx_box*dx_box + dy_box*dy_box + dz_box*dz_box);
            L = diag * 2.0;
            if (L < 100.0) L = 100.0;

            world_mesh = std::make_unique<Mesh>(boolean::Engine::geometry_to_mesh_ik(world_geo));
            if (CGAL::is_closed(*world_mesh)) {
                CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(*world_mesh);
            }
            tree = std::make_unique<Tree>(CGAL::faces(*world_mesh).first, CGAL::faces(*world_mesh).second, *world_mesh);
            tree->build();
        }

        Shape out = analyze_shape_recursive(vfs, in, Matrix::identity(), dx, dy, dz, sin_alpha, L, tree.get(), world_mesh.get());
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "dx", "dy", "dz", "min_draft"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/undercut"},
            {"description", "Analyzes the geometry of a shape for undercuts relative to a pull direction, splitting the mesh into components colored green (safe), red (undercut), and yellow (flat/draft boundary/trapped)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to analyze."}}}}},
            {"arguments", {
                {{"name", "dx"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "X component of the pull direction vector."}},
                {{"name", "dy"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Y component of the pull direction vector."}},
                {{"name", "dz"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Z component of the pull direction vector."}},
                {{"name", "min_draft"}, {"type", "jot:number"}, {"default", 0.5}, {"description", "Minimum draft angle in degrees."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void undercut_init(fs::VFSNode* vfs) {
    Processor::register_op<UndercutOp<>, Shape, double, double, double, double>(vfs, "jot/undercut");
}

} // namespace geo
} // namespace jotcad
