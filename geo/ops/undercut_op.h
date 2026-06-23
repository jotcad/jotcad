#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "triangulation.h"
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct UndercutOp : P {
    static constexpr const char* path = "jot/undercut";

    static Shape analyze_shape_recursive(fs::VFSNode* vfs, const Shape& in, double dx, double dy, double dz) {
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

            // 1. Process explicit triangles
            if (!geo.triangles.empty()) {
                for (const auto& tri : geo.triangles) {
                    int i0 = tri[0];
                    int i1 = tri[1];
                    int i2 = tri[2];
                    
                    double p0_x = CGAL::to_double(geo.vertices[i0].x);
                    double p0_y = CGAL::to_double(geo.vertices[i0].y);
                    double p0_z = CGAL::to_double(geo.vertices[i0].z);

                    double p1_x = CGAL::to_double(geo.vertices[i1].x);
                    double p1_y = CGAL::to_double(geo.vertices[i1].y);
                    double p1_z = CGAL::to_double(geo.vertices[i1].z);

                    double p2_x = CGAL::to_double(geo.vertices[i2].x);
                    double p2_y = CGAL::to_double(geo.vertices[i2].y);
                    double p2_z = CGAL::to_double(geo.vertices[i2].z);

                    double ux = p1_x - p0_x;
                    double uy = p1_y - p0_y;
                    double uz = p1_z - p0_z;

                    double vx = p2_x - p0_x;
                    double vy = p2_y - p0_y;
                    double vz = p2_z - p0_z;

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
                    
                    if (dot > 1e-5) {
                        add_triangle_to_geo(safe_geo, safe_vmap, i0, i1, i2);
                    } else if (dot < -1e-5) {
                        add_triangle_to_geo(undercut_geo, undercut_vmap, i0, i1, i2);
                    } else {
                        add_triangle_to_geo(flat_geo, flat_vmap, i0, i1, i2);
                    }
                }
            } else if (!geo.faces.empty()) {
                // 2. Process general faces (by triangulating them first)
                std::vector<Vec3> pts;
                for (const auto& v : geo.vertices) {
                    pts.push_back(Vec3{CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
                }

                for (const auto& f : geo.faces) {
                    Triangulation::triangulate_face(f, pts, [&](int i0, int i1, int i2) {
                        double p0_x = pts[i0].x;
                        double p0_y = pts[i0].y;
                        double p0_z = pts[i0].z;

                        double p1_x = pts[i1].x;
                        double p1_y = pts[i1].y;
                        double p1_z = pts[i1].z;

                        double p2_x = pts[i2].x;
                        double p2_y = pts[i2].y;
                        double p2_z = pts[i2].z;

                        double ux = p1_x - p0_x;
                        double uy = p1_y - p0_y;
                        double uz = p1_z - p0_z;

                        double vx = p2_x - p0_x;
                        double vy = p2_y - p0_y;
                        double vz = p2_z - p0_z;

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
                        
                        if (dot > 1e-5) {
                            add_triangle_to_geo(safe_geo, safe_vmap, i0, i1, i2);
                        } else if (dot < -1e-5) {
                            add_triangle_to_geo(undercut_geo, undercut_vmap, i0, i1, i2);
                        } else {
                            add_triangle_to_geo(flat_geo, flat_vmap, i0, i1, i2);
                        }
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
            out.components.push_back(analyze_shape_recursive(vfs, child, dx, dy, dz));
        }
        
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double dx, double dy, double dz) {
        // Normalize pull vector direction
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 1e-12) {
            dx /= len;
            dy /= len;
            dz /= len;
        } else {
            dx = 0.0; dy = 0.0; dz = 1.0; // Default: Z-up
        }

        Shape out = analyze_shape_recursive(vfs, in, dx, dy, dz);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "dx", "dy", "dz"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/undercut"},
            {"description", "Analyzes the geometry of a shape for undercuts relative to a pull direction, splitting the mesh into components colored green (safe), red (undercut), and yellow (flat/draft boundary)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to analyze."}}}}},
            {"arguments", {
                {{"name", "dx"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "X component of the pull direction vector."}},
                {{"name", "dy"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Y component of the pull direction vector."}},
                {{"name", "dz"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Z component of the pull direction vector."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void undercut_init(fs::VFSNode* vfs) {
    Processor::register_op<UndercutOp<>, Shape, double, double, double>(vfs, "jot/undercut");
}

} // namespace geo
} // namespace jotcad
