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

    // 1. Identify 1-ring local peak vertices in +Z
    std::vector<bool> is_peak(pts.size(), false);
    std::vector<int> peak_indices;

    for (size_t i = 0; i < pts.size(); ++i) {
        bool peak = true;
        for (int n_idx : neighbors[i]) {
            if (pts[n_idx].z() > pts[i].z()) {
                peak = false;
                break;
            }
        }
        if (peak) {
            is_peak[i] = true;
            peak_indices.push_back((int)i);
        }
    }

    // 2. Cluster adjacent peak vertices
    std::vector<bool> visited(pts.size(), false);
    std::vector<PeakCluster> clusters;

    for (int p_idx : peak_indices) {
        if (visited[p_idx]) continue;

        PeakCluster cluster;
        cluster.id = (int)clusters.size() + 1;

        std::queue<int> q;
        q.push(p_idx);
        visited[p_idx] = true;

        EK::Point_3 apex_pt = pts[p_idx];
        FT max_z = pts[p_idx].z();

        while (!q.empty()) {
            int curr = q.front();
            q.pop();

            cluster.vertices.push_back(ExactMesh::Vertex_index(curr));
            if (pts[curr].z() > max_z) {
                max_z = pts[curr].z();
                apex_pt = pts[curr];
            }

            for (int n_idx : neighbors[curr]) {
                if (is_peak[n_idx] && !visited[n_idx]) {
                    visited[n_idx] = true;
                    q.push(n_idx);
                }
            }
        }

        cluster.apex = apex_pt;
        cluster.max_z = max_z;
        clusters.push_back(cluster);
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
