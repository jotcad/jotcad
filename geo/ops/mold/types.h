#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include "fix/kiss.h"
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

// Symbolic constant for expanding 2D arrangement zero-area pinches into positive-area bridges
constexpr double kPinchBridgeWidthMM = 0.01;
inline FT pinch_bridge_width_ft() { return FT(1) / FT(100); }

namespace optimizer_constants {
    constexpr double UNHANDLED_RESIDUE_PENALTY_WEIGHT = 10.0;
    constexpr double PRIOR_ENCROACHMENT_PENALTY_WEIGHT = 3.0;
    constexpr double BASE_DEMOLDABILITY_WEIGHT = 0.5;
}

struct MoldParams {
    FT padding = FT(10);
    FT explode = FT(0);
    FT draft = FT(0); // Default 0.0 draft (strictly on or above silhouette horizon)
    fix::KissMode kiss_mode = fix::KissMode::WELD;
    FT kiss_width = pinch_bridge_width_ft();
    bool lines = true;
    bool molds = true;
};

struct MoldPiece {
    ExactMesh mesh;
    EK::Vector_3 draw_vector;
    std::string name;
    std::string color;
    int mold_piece;
};

struct EnvelopeWedgeResult {
    ExactMesh solid_wedge;
    std::set<size_t> source_faces;
    FT total_area = FT(0);
    std::vector<std::vector<Point_3>> boundary_loops_3d;
};

struct TideParams {
    bool enabled = false;
    FT u_min = FT(0), u_max = FT(0);
    FT v_min = FT(0), v_max = FT(0);
    FT z_margin = FT(0);
    FT z_top = FT(0);
};


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
