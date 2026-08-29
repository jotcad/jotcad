#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "boolean/engine.h"
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace jotcad {
namespace geo {
namespace mold {

typedef boolean::ExactMesh ExactMesh;
typedef ExactMesh::Property_map<ExactMesh::Face_index, bool> FaceBoolMap;
typedef CGAL::AABB_face_graph_triangle_primitive<ExactMesh> Primitive;
typedef CGAL::AABB_traits_3<EK, Primitive> Traits;
typedef CGAL::AABB_tree<Traits> Tree;

struct DSU {
    std::vector<int> parent;
    DSU(int n) {
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
        if (root_i != root_j) parent[root_i] = root_j;
    }
};

struct EdgeKey {
    int u, v;
    bool operator<(const EdgeKey& o) const {
        if (u != o.u) return u < o.u;
        return v < o.v;
    }
};

struct UndercutCluster {
    std::vector<int> face_indices;
    EK::Vector_3 avg_normal;
    FT xmin, xmax, ymin, ymax, zmin, zmax;
};

struct MoldParams {
    FT padding = FT(10);
    FT explode = FT(0);
    FT draft = FT(0);
};

struct MoldPiece {
    ExactMesh mesh;
    EK::Vector_3 draw_vector;
    std::string name;
    std::string color;
    int mold_piece;
};

// Helper: Collect world geometry recursively across scene graph
inline void collect_world_geometry_recursive(fs::VFSNode* vfs, const Shape& s, const Matrix& current_tf, Geometry& world_geo) {
    if (s.geometry.has_value()) {
        Geometry geo = JotVfsProtocol::read_shape_geo(vfs, s);
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

// Helper: Build exact rational bounding box in pure FT
inline Geometry build_box_geo(FT xmin, FT xmax, FT ymin, FT ymax, FT zmin, FT zmax) {
    Geometry geo;
    geo.vertices.push_back({xmin, ymin, zmin}); // 0
    geo.vertices.push_back({xmax, ymin, zmin}); // 1
    geo.vertices.push_back({xmax, ymax, zmin}); // 2
    geo.vertices.push_back({xmin, ymax, zmin}); // 3
    geo.vertices.push_back({xmin, ymin, zmax}); // 4
    geo.vertices.push_back({xmax, ymin, zmax}); // 5
    geo.vertices.push_back({xmax, ymax, zmax}); // 6
    geo.vertices.push_back({xmin, ymax, zmax}); // 7

    geo.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
    geo.faces.push_back({{{4, 5, 6, 7}}}); // Top
    geo.faces.push_back({{{0, 1, 5, 4}}}); // Front
    geo.faces.push_back({{{1, 2, 6, 5}}}); // Right
    geo.faces.push_back({{{2, 3, 7, 6}}}); // Back
    geo.faces.push_back({{{3, 0, 4, 7}}}); // Left

    geo.triangles.push_back({3, 2, 1}); geo.triangles.push_back({3, 1, 0});
    geo.triangles.push_back({4, 5, 6}); geo.triangles.push_back({4, 6, 7});
    geo.triangles.push_back({0, 1, 5}); geo.triangles.push_back({0, 5, 4});
    geo.triangles.push_back({1, 2, 6}); geo.triangles.push_back({1, 6, 5});
    geo.triangles.push_back({2, 3, 7}); geo.triangles.push_back({2, 7, 6});
    geo.triangles.push_back({3, 0, 4}); geo.triangles.push_back({3, 4, 7});
    return geo;
}

struct OrientedBox {
    EK::Vector_3 u, v, w;
    FT u_min, u_max;
    FT v_min, v_max;
    FT w_min, w_max;
    FT volume = FT(0);

    Geometry to_geometry() const {
        auto make_pt = [&](FT u_val, FT v_val, FT w_val) -> Vertex {
            FT px = u_val * u.x() + v_val * v.x() + w_val * w.x();
            FT py = u_val * u.y() + v_val * v.y() + w_val * w.y();
            FT pz = u_val * u.z() + v_val * v.z() + w_val * w.z();
            return Vertex{px, py, pz};
        };

        Geometry geo;
        geo.vertices.push_back(make_pt(u_min, v_min, w_min)); // 0
        geo.vertices.push_back(make_pt(u_max, v_min, w_min)); // 1
        geo.vertices.push_back(make_pt(u_max, v_max, w_min)); // 2
        geo.vertices.push_back(make_pt(u_min, v_max, w_min)); // 3
        geo.vertices.push_back(make_pt(u_min, v_min, w_max)); // 4
        geo.vertices.push_back(make_pt(u_max, v_min, w_max)); // 5
        geo.vertices.push_back(make_pt(u_max, v_max, w_max)); // 6
        geo.vertices.push_back(make_pt(u_min, v_max, w_max)); // 7

        geo.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
        geo.faces.push_back({{{4, 5, 6, 7}}}); // Top
        geo.faces.push_back({{{0, 1, 5, 4}}}); // Front
        geo.faces.push_back({{{1, 2, 6, 5}}}); // Right
        geo.faces.push_back({{{2, 3, 7, 6}}}); // Back
        geo.faces.push_back({{{3, 0, 4, 7}}}); // Left

        geo.triangles.push_back({3, 2, 1}); geo.triangles.push_back({3, 1, 0});
        geo.triangles.push_back({4, 5, 6}); geo.triangles.push_back({4, 6, 7});
        geo.triangles.push_back({0, 1, 5}); geo.triangles.push_back({0, 5, 4});
        geo.triangles.push_back({1, 2, 6}); geo.triangles.push_back({1, 6, 5});
        geo.triangles.push_back({2, 3, 7}); geo.triangles.push_back({2, 7, 6});
        geo.triangles.push_back({3, 0, 4}); geo.triangles.push_back({3, 4, 7});
        return geo;
    }
};

inline OrientedBox compute_min_volume_obb(
    const ExactMesh& mesh_part,
    FT padding,
    const std::vector<EK::Vector_3>& preferred_dirs = {}
) {
    std::vector<EK::Point_3> pts;
    pts.reserve(mesh_part.number_of_vertices());
    for (auto v : mesh_part.vertices()) {
        pts.push_back(mesh_part.point(v));
    }

    if (pts.empty()) {
        return {};
    }

    std::vector<EK::Vector_3> candidate_axes;
    candidate_axes.push_back(EK::Vector_3(FT(1), FT(0), FT(0)));
    candidate_axes.push_back(EK::Vector_3(FT(0), FT(1), FT(0)));
    candidate_axes.push_back(EK::Vector_3(FT(0), FT(0), FT(1)));

    for (const auto& d : preferred_dirs) {
        double len = std::sqrt(CGAL::to_double(d.squared_length()));
        if (len > 1e-6) {
            candidate_axes.push_back(EK::Vector_3(
                FT(CGAL::to_double(d.x()) / len),
                FT(CGAL::to_double(d.y()) / len),
                FT(CGAL::to_double(d.z()) / len)
            ));
        }
    }

    for (auto f : CGAL::faces(mesh_part)) {
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        EK::Vector_3 fn = CGAL::normal(p0, p1, p2);
        double len = std::sqrt(CGAL::to_double(fn.squared_length()));
        if (len > 1e-6) {
            candidate_axes.push_back(EK::Vector_3(
                FT(CGAL::to_double(fn.x()) / len),
                FT(CGAL::to_double(fn.y()) / len),
                FT(CGAL::to_double(fn.z()) / len)
            ));
        }
    }

    OrientedBox best_obb;
    FT best_volume = -1;

    for (const auto& w_axis : candidate_axes) {
        EK::Vector_3 u_init;
        if (std::abs(CGAL::to_double(w_axis.x())) < 0.9) {
            u_init = CGAL::cross_product(w_axis, EK::Vector_3(FT(1), FT(0), FT(0)));
        } else {
            u_init = CGAL::cross_product(w_axis, EK::Vector_3(FT(0), FT(1), FT(0)));
        }
        double u_len = std::sqrt(CGAL::to_double(u_init.squared_length()));
        if (u_len < 1e-6) continue;
        u_init = EK::Vector_3(FT(CGAL::to_double(u_init.x()) / u_len), FT(CGAL::to_double(u_init.y()) / u_len), FT(CGAL::to_double(u_init.z()) / u_len));
        EK::Vector_3 v_init = CGAL::cross_product(w_axis, u_init);

        const int STEPS = 36;
        for (int s = 0; s < STEPS; ++s) {
            double theta = (M_PI / 2.0) * (s / double(STEPS));
            double c = std::cos(theta), sn = std::sin(theta);

            EK::Vector_3 u_vec = EK::Vector_3(
                FT(c * CGAL::to_double(u_init.x()) + sn * CGAL::to_double(v_init.x())),
                FT(c * CGAL::to_double(u_init.y()) + sn * CGAL::to_double(v_init.y())),
                FT(c * CGAL::to_double(u_init.z()) + sn * CGAL::to_double(v_init.z()))
            );
            EK::Vector_3 v_vec = CGAL::cross_product(w_axis, u_vec);

            FT u_min, u_max, v_min, v_max, w_min, w_max;
            bool first = true;
            for (const auto& p : pts) {
                FT u_val = p.x() * u_vec.x() + p.y() * u_vec.y() + p.z() * u_vec.z();
                FT v_val = p.x() * v_vec.x() + p.y() * v_vec.y() + p.z() * v_vec.z();
                FT w_val = p.x() * w_axis.x() + p.y() * w_axis.y() + p.z() * w_axis.z();

                if (first) {
                    u_min = u_max = u_val;
                    v_min = v_max = v_val;
                    w_min = w_max = w_val;
                    first = false;
                } else {
                    u_min = (std::min)(u_min, u_val); u_max = (std::max)(u_max, u_val);
                    v_min = (std::min)(v_min, v_val); v_max = (std::max)(v_max, v_val);
                    w_min = (std::min)(w_min, w_val); w_max = (std::max)(w_max, w_val);
                }
            }

            u_min -= padding; u_max += padding;
            v_min -= padding; v_max += padding;
            w_min -= padding; w_max += padding;

            FT vol = (u_max - u_min) * (v_max - v_min) * (w_max - w_min);
            if (best_volume < FT(0) || vol < best_volume) {
                best_volume = vol;
                best_obb = {u_vec, v_vec, w_axis, u_min, u_max, v_min, v_max, w_min, w_max, vol};
            }
        }
    }
    return best_obb;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
