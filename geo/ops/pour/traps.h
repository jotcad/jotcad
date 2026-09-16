#pragma once
#include "types.h"
#include <map>
#include <set>
#include <queue>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace pour {

inline std::vector<PeakCluster> detect_peaks_and_air_traps(const ExactMesh& mesh) {
    std::vector<EK::Point_3> pts;
    pts.reserve(mesh.number_of_vertices());
    for (auto v : mesh.vertices()) {
        pts.push_back(mesh.point(v));
    }

    std::vector<std::vector<int>> neighbors(mesh.number_of_vertices());
    for (auto e : mesh.edges()) {
        auto h = mesh.halfedge(e);
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        neighbors[u].push_back(v);
        neighbors[v].push_back(u);
    }

    // 1. Identify plateau-connected summit components in +Z
    std::vector<bool> visited(pts.size(), false);
    std::vector<PeakCluster> clusters;

    for (size_t i = 0; i < pts.size(); ++i) {
        if (visited[i]) continue;

        // BFS across connected vertices of equal elevation (plateau)
        std::vector<int> component;
        std::queue<int> q;
        q.push((int)i);
        visited[i] = true;
        FT plateau_z = pts[i].z();
        bool has_higher_neighbor = false;

        while (!q.empty()) {
            int curr = q.front();
            q.pop();
            component.push_back(curr);

            for (int n_idx : neighbors[curr]) {
                if (pts[n_idx].z() > plateau_z) {
                    has_higher_neighbor = true;
                } else if (pts[n_idx].z() == plateau_z && !visited[n_idx]) {
                    visited[n_idx] = true;
                    q.push(n_idx);
                }
            }
        }

        // A plateau forms an air trap / summit IF AND ONLY IF no neighbor is strictly higher
        if (!has_higher_neighbor) {
            PeakCluster cluster;
            cluster.id = (int)clusters.size() + 1;
            cluster.max_z = plateau_z;
            cluster.apex = pts[component[0]];
            for (int v_idx : component) {
                cluster.vertices.push_back(ExactMesh::Vertex_index(v_idx));
            }
            clusters.push_back(cluster);
        }
    }

    // 3. Sort clusters by height descending: highest cluster is primary pour gate
    std::sort(clusters.begin(), clusters.end(), [](const PeakCluster& a, const PeakCluster& b) {
        return a.max_z > b.max_z;
    });

    if (!clusters.empty()) {
        clusters[0].is_primary = true;
    }

    return clusters;
}

} // namespace pour
} // namespace geo
} // namespace jotcad
