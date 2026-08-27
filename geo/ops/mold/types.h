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

} // namespace mold
} // namespace geo
} // namespace jotcad
