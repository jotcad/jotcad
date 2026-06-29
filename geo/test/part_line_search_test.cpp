#include "test_base.h"
#include "infra/stl.h"
#include <fstream>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;

struct DSU {
    std::vector<int> parent;
    DSU(int n) {
        parent.resize(n);
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int i) {
        if (parent[i] == i)
            return i;
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

struct SearchResult {
    double dx, dy, dz;
    int loop_count;
    int segment_count;

    bool operator<(const SearchResult& other) const {
        if (loop_count != other.loop_count) {
            return loop_count < other.loop_count;
        }
        return segment_count < other.segment_count;
    }
};

void run_part_line_search() {
    // 1. Load the bear STL
    std::string bear_path = "scratch/bear.stl";
    {
        std::ifstream check(bear_path);
        if (!check.good()) {
            check.open("../scratch/bear.stl");
            if (check.good()) {
                bear_path = "../scratch/bear.stl";
            } else {
                bear_path = "../../scratch/bear.stl";
            }
        }
    }
    Geometry bear_geo;
    if (!STLReader::read_file(bear_path, bear_geo)) {
        std::cerr << "Could not load bear.stl from " << bear_path << std::endl;
        return;
    }
    std::cout << "Loaded bear.stl with " << bear_geo.triangles.size() << " triangles." << std::endl;

    // 2. Precompute face normals
    std::vector<IK::Vector_3> face_normals(bear_geo.triangles.size());
    for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        const auto& p0 = bear_geo.vertices[tri[0]];
        const auto& p1 = bear_geo.vertices[tri[1]];
        const auto& p2 = bear_geo.vertices[tri[2]];
        
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

    // 3. Prebuild edge-to-faces map
    struct Edge {
        int u, v;
        bool operator<(const Edge& other) const {
            if (u != other.u) return u < other.u;
            return v < other.v;
        }
    };

    std::map<Edge, std::vector<int>> edge_to_faces;
    for (int f_idx = 0; f_idx < (int)bear_geo.triangles.size(); ++f_idx) {
        const auto& tri = bear_geo.triangles[f_idx];
        for (int i = 0; i < 3; ++i) {
            int u = tri[i];
            int v = tri[(i + 1) % 3];
            Edge edge = {std::min(u, v), std::max(u, v)};
            edge_to_faces[edge].push_back(f_idx);
        }
    }

    // 4. Sample directions on the unit sphere
    std::vector<IK::Vector_3> directions;
    int theta_steps = 90;
    int phi_steps = 180;
    for (int i = 1; i < theta_steps; ++i) {
        double theta = M_PI * i / theta_steps;
        double sin_t = std::sin(theta);
        double cos_t = std::cos(theta);
        for (int j = 0; j < phi_steps; ++j) {
            double phi = 2.0 * M_PI * j / phi_steps;
            double dx = sin_t * std::cos(phi);
            double dy = sin_t * std::sin(phi);
            double dz = cos_t;
            directions.push_back(IK::Vector_3(dx, dy, dz));
        }
    }
    directions.push_back(IK::Vector_3(0, 0, 1));
    directions.push_back(IK::Vector_3(0, 0, -1));

    std::cout << "Evaluating " << directions.size() << " directions..." << std::endl;

    std::vector<SearchResult> results;
    results.reserve(directions.size());

    // Pre-allocate structures for DSU to avoid allocation inside the loop
    int vertex_count = (int)bear_geo.vertices.size();

    // 5. Run the search
    for (const auto& d : directions) {
        // Precompute dots
        std::vector<double> dots(bear_geo.triangles.size());
        for (size_t f_idx = 0; f_idx < bear_geo.triangles.size(); ++f_idx) {
            dots[f_idx] = CGAL::to_double(face_normals[f_idx] * d);
        }

        // Collect parting line edges and build DSU
        DSU dsu(vertex_count);
        std::vector<bool> used_vertices(vertex_count, false);
        int segment_count = 0;

        for (const auto& [edge, faces] : edge_to_faces) {
            bool is_part_line = false;
            if (faces.size() == 1) {
                is_part_line = true;
            } else if (faces.size() >= 2) {
                bool has_pos = false;
                bool has_neg = false;
                for (int f_idx : faces) {
                    double dot = dots[f_idx];
                    if (dot > 1e-9) has_pos = true;
                    else if (dot < -1e-9) has_neg = true;
                }
                if (has_pos && has_neg) {
                    is_part_line = true;
                }
            }

            if (is_part_line) {
                dsu.unite(edge.u, edge.v);
                used_vertices[edge.u] = true;
                used_vertices[edge.v] = true;
                segment_count++;
            }
        }

        // Count unique loops
        std::set<int> unique_roots;
        for (int i = 0; i < vertex_count; ++i) {
            if (used_vertices[i]) {
                unique_roots.insert(dsu.find(i));
            }
        }
        int loop_count = (int)unique_roots.size();

        results.push_back({CGAL::to_double(d.x()), CGAL::to_double(d.y()), CGAL::to_double(d.z()), loop_count, segment_count});
    }

    // 6. Sort results to find optimal orientations
    std::sort(results.begin(), results.end());

    std::cout << "\nTop 15 Optimal Orientations (minimizing loops, then segment count):" << std::endl;
    std::cout << "Rank | Direction Vector (dx, dy, dz) | Loops | Segments" << std::endl;
    std::cout << "-----|-------------------------------|-------|---------" << std::endl;
    for (int r = 0; r < 15 && r < (int)results.size(); ++r) {
        const auto& res = results[r];
        printf("%4d | [%7.4f, %7.4f, %7.4f]      | %5d | %8d\n", 
               r + 1, res.dx, res.dy, res.dz, res.loop_count, res.segment_count);
    }
}

int main() {
    run_part_line_search();
    return 0;
}
