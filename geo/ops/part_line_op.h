#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "triangulation.h"
#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <memory>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PartLineOp : P {
    static constexpr const char* path = "jot/partLine";

    static bool is_tangled(const std::vector<std::pair<int, int>>& parting_segments, const Geometry& world_geo, const IK::Vector_3& dir) {
        typedef CGAL::Arr_segment_traits_2<EK> Traits_2;
        typedef CGAL::Arrangement_2<Traits_2> Arrangement;

        EK::Vector_3 dir_ek(dir.x(), dir.y(), dir.z());
        EK::Plane_3 plane(EK::Point_3(0, 0, 0), dir_ek);

        Arrangement arr;
        for (const auto& seg : parting_segments) {
            const auto& p1_raw = world_geo.vertices[seg.first];
            const auto& p2_raw = world_geo.vertices[seg.second];
            EK::Point_3 p1(p1_raw.x, p1_raw.y, p1_raw.z);
            EK::Point_3 p2(p2_raw.x, p2_raw.y, p2_raw.z);
            
            if (p1 == p2) continue;
            
            Traits_2::Curve_2 curve(plane.to_2d(p1), plane.to_2d(p2));
            CGAL::insert(arr, curve);
        }

        for (auto v = arr.vertices_begin(); v != arr.vertices_end(); ++v) {
            if (v->degree() != 2) {
                return true;
            }
        }
        return false;
    }

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

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double dx, double dy, double dz, bool optimize = false) {
        // 1. Flatten all geometry in world coordinates
        Geometry world_geo;
        collect_world_geometry_recursive(vfs, in, Matrix::identity(), world_geo);

        // 2. Compute face normals
        std::vector<IK::Vector_3> face_normals(world_geo.triangles.size());
        for (size_t f_idx = 0; f_idx < world_geo.triangles.size(); ++f_idx) {
            const auto& tri = world_geo.triangles[f_idx];
            const auto& p0 = world_geo.vertices[tri[0]];
            const auto& p1 = world_geo.vertices[tri[1]];
            const auto& p2 = world_geo.vertices[tri[2]];
            
            double ux = CGAL::to_double(p1.x - p0.x);
            double uy = CGAL::to_double(p1.y - p0.y);
            double uz = CGAL::to_double(p1.z - p0.z);

            double vx = CGAL::to_double(p2.x - p0.x);
            double vy = CGAL::to_double(p2.y - p0.y);
            double vz = CGAL::to_double(p2.z - p0.z);

            double nx = uy * vz - uz * vy;
            double ny = uz * vx - ux * vz;
            double nz = ux * vy - uy * vx;

            double flen = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (flen > 1e-12) {
                nx /= flen; ny /= flen; nz /= flen;
            }
            face_normals[f_idx] = IK::Vector_3(nx, ny, nz);
        }

        // 3. Build edge-to-faces map
        struct Edge {
            int u, v;
            bool operator<(const Edge& other) const {
                if (u != other.u) return u < other.u;
                return v < other.v;
            }
        };

        std::map<Edge, std::vector<int>> edge_to_faces;
        for (int f_idx = 0; f_idx < (int)world_geo.triangles.size(); ++f_idx) {
            const auto& tri = world_geo.triangles[f_idx];
            for (int i = 0; i < 3; ++i) {
                int u = tri[i];
                int v = tri[(i + 1) % 3];
                Edge edge = {std::min(u, v), std::max(u, v)};
                edge_to_faces[edge].push_back(f_idx);
            }
        }

        // 4. Perform search/optimization if requested
        if (optimize && !world_geo.triangles.empty()) {
            std::vector<IK::Vector_3> directions;
            int theta_steps = 45;
            int phi_steps = 90;
            for (int i = 1; i < theta_steps; ++i) {
                double theta = M_PI * i / theta_steps;
                double sin_t = std::sin(theta);
                double cos_t = std::cos(theta);
                for (int j = 0; j < phi_steps; ++j) {
                    double phi = 2.0 * M_PI * j / phi_steps;
                    double sx = sin_t * std::cos(phi);
                    double sy = sin_t * std::sin(phi);
                    double sz = cos_t;
                    directions.push_back(IK::Vector_3(sx, sy, sz));
                }
            }
            directions.push_back(IK::Vector_3(0, 0, 1));
            directions.push_back(IK::Vector_3(0, 0, -1));

            int vertex_count = (int)world_geo.vertices.size();
            bool best_is_tangle_free = false;
            double best_dx = 0.0, best_dy = 0.0, best_dz = 1.0;
            int min_loops = 999999;
            int min_segments = 999999;

            struct LocalDSU {
                std::vector<int> parent;
                LocalDSU(int n) {
                    parent.resize(n);
                    for (int i = 0; i < n; ++i) parent[i] = i;
                }
                int find(int i) {
                    if (parent[i] == i) return i;
                    return parent[i] = find(parent[i]);
                }
                void unite(int i, int j) {
                    int root_i = find(i);
                    int root_j = find(j);
                    if (root_i != root_j) {
                        parent[root_i] = root_j;
                    }
                }
            };

            for (const auto& d : directions) {
                std::vector<double> dots(world_geo.triangles.size());
                for (size_t f_idx = 0; f_idx < world_geo.triangles.size(); ++f_idx) {
                    dots[f_idx] = CGAL::to_double(face_normals[f_idx] * d);
                }

                LocalDSU dsu(vertex_count);
                std::vector<bool> used_vertices(vertex_count, false);
                int segment_count = 0;
                std::vector<std::pair<int, int>> parting_segments;

                for (const auto& [edge, faces] : edge_to_faces) {
                    bool is_part_line = false;
                    if (faces.size() == 1) {
                        is_part_line = true;
                    } else if (faces.size() >= 2) {
                        double dot1 = dots[faces[0]];
                        double dot2 = dots[faces[1]];
                        if ((dot1 < 0.0) != (dot2 < 0.0)) {
                            is_part_line = true;
                        }
                    }

                    if (is_part_line) {
                        dsu.unite(edge.u, edge.v);
                        used_vertices[edge.u] = true;
                        used_vertices[edge.v] = true;
                        segment_count++;
                        parting_segments.push_back({edge.u, edge.v});
                    }
                }

                std::set<int> unique_roots;
                for (int i = 0; i < vertex_count; ++i) {
                    if (used_vertices[i]) {
                        unique_roots.insert(dsu.find(i));
                    }
                }
                int loop_count = (int)unique_roots.size();

                bool tangle_free = false;
                if (loop_count > 0) {
                    tangle_free = !is_tangled(parting_segments, world_geo, d);
                }

                bool update = false;
                if (loop_count > 0) {
                    if (!best_is_tangle_free && tangle_free) {
                        update = true;
                    } else if (best_is_tangle_free == tangle_free) {
                        if (loop_count < min_loops) {
                            update = true;
                        } else if (loop_count == min_loops && segment_count < min_segments) {
                            update = true;
                        }
                    }
                }

                if (update) {
                    min_loops = loop_count;
                    min_segments = segment_count;
                    best_is_tangle_free = tangle_free;
                    best_dx = CGAL::to_double(d.x());
                    best_dy = CGAL::to_double(d.y());
                    best_dz = CGAL::to_double(d.z());
                    if (tangle_free) {
                        std::cout << "    [Optimizer] Found tangle-free direction: [" 
                                  << best_dx << ", " << best_dy << ", " << best_dz 
                                  << "] with loops=" << loop_count << std::endl;
                    }
                }
            }

            dx = best_dx;
            dy = best_dy;
            dz = best_dz;
        }

        // Normalize final vector
        double len = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (len > 1e-12) {
            dx /= len;
            dy /= len;
            dz /= len;
        } else {
            dx = 0.0; dy = 0.0; dz = 1.0;
        }

        IK::Vector_3 dir_ik(dx, dy, dz);

        // 5. Extract parting line edges using finalized direction
        Geometry line_geo;
        std::map<int, int> v_map;
        auto get_or_add_vertex = [&](int old_idx) -> int {
            auto it = v_map.find(old_idx);
            if (it != v_map.end()) return it->second;
            int new_idx = (int)line_geo.vertices.size();
            line_geo.vertices.push_back(world_geo.vertices[old_idx]);
            v_map[old_idx] = new_idx;
            return new_idx;
        };

        for (const auto& [edge, faces] : edge_to_faces) {
            bool is_part_line = false;
            if (faces.size() == 1) {
                is_part_line = true;
            } else if (faces.size() >= 2) {
                double dot1 = CGAL::to_double(face_normals[faces[0]] * dir_ik);
                double dot2 = CGAL::to_double(face_normals[faces[1]] * dir_ik);
                if ((dot1 < 0.0) != (dot2 < 0.0)) {
                    is_part_line = true;
                }
            }
            if (is_part_line) {
                int u = get_or_add_vertex(edge.u);
                int v = get_or_add_vertex(edge.v);
                line_geo.segments.push_back({u, v});
            }
        }

        Shape out = P::make_shape(vfs, line_geo, {
            {"color", "#2bee2b"}, 
            {"name", "parting_line"}, 
            {"type", "outline"},
            {"dx", dx},
            {"dy", dy},
            {"dz", dz}
        });
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "dx", "dy", "dz", "optimize"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/partLine"},
            {"description", "Generates the parting line (silhouette curve) of a shape relative to a pull direction vector (or automatically searches for the optimal direction vector), outputting a set of 3D line segments."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to extract parting line from."}}}}},
            {"arguments", {
                {{"name", "dx"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "X component of the pull direction vector."}},
                {{"name", "dy"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Y component of the pull direction vector."}},
                {{"name", "dz"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Z component of the pull direction vector."}},
                {{"name", "optimize"}, {"type", "jot:boolean"}, {"default", false}, {"description", "If true, automatically find the optimal pull direction that minimizes parting line loops."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void part_line_init(fs::VFSNode* vfs) {
    Processor::register_op<PartLineOp<>, Shape, double, double, double, bool>(vfs, "jot/partLine");
}

} // namespace geo
} // namespace jotcad
