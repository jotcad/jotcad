#pragma once
#include "types.h"
#include <map>
#include <set>
#include <queue>
#include <vector>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace pour {

/**
 * Implements the Hydraulic Watershed DAG from Section 3 of docs/POUR_PREP_DRAINAGE_DESIGN.md.
 * 
 * 1. Identifies the global summit G = argmax_v Z(v) as the primary pour gate.
 * 2. Constructs an ascending directed graph of edges where bubble ascent is physically
 *    admissible: dz > 0 and (dz / dL) >= sin(min_angle).
 * 3. Performs reverse reachability from G: any vertex that cannot reach G along an
 *    admissible ascending path constitutes a true hydraulic air trap.
 * 4. Clusters undrained vertices into connected trap components and flags the highest
 *    point of each component as an auxiliary vent apex.
 */
inline std::vector<PeakCluster> detect_peaks_and_air_traps(const ExactMesh& mesh, double min_angle = default_min_angle) {
    if (mesh.number_of_vertices() == 0) return {};

    std::vector<EK::Point_3> pts;
    pts.reserve(mesh.number_of_vertices());
    FT z_max = FT(-1e18);
    int g_apex = 0;

    for (auto v : mesh.vertices()) {
        EK::Point_3 p = mesh.point(v);
        pts.push_back(p);
        if (p.z() > z_max) {
            z_max = p.z();
            g_apex = (int)pts.size() - 1;
        }
    }

    // Minimum slope threshold for bubble detachment (sin(min_angle))
    double sin_min = std::sin(min_angle * 2.0 * M_PI);
    FT sin_sq = FT(sin_min * sin_min);

    // Build undirected mesh adjacency and directed ascending drainage graph
    int n_verts = (int)pts.size();
    std::vector<std::vector<int>> mesh_neighbors(n_verts);
    // ascending_predecessors[v] stores all vertices u such that u -> v is an admissible ascending drainage edge
    std::vector<std::vector<int>> ascending_predecessors(n_verts);

    for (auto e : mesh.edges()) {
        auto h = mesh.halfedge(e);
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);

        mesh_neighbors[u].push_back(v);
        mesh_neighbors[v].push_back(u);

        FT dz = pts[v].z() - pts[u].z();
        FT dx = pts[v].x() - pts[u].x();
        FT dy = pts[v].y() - pts[u].y();
        FT dist_sq = dx * dx + dy * dy + dz * dz;

        if (dist_sq < FT(1e-12)) continue; // Degenerate / coincident vertices

        // Check u -> v: ascending step
        if (dz > FT(0)) {
            FT dz_sq = dz * dz;
            if (dz_sq >= sin_sq * dist_sq) {
                ascending_predecessors[v].push_back(u);
            }
        }
        // Check v -> u: ascending step
        else if (dz < FT(0)) {
            FT neg_dz = -dz;
            FT dz_sq = neg_dz * neg_dz;
            if (dz_sq >= sin_sq * dist_sq) {
                ascending_predecessors[u].push_back(v);
            }
        }
    }

    // Reverse reachability from single primary gate component G (containing g_apex)
    std::vector<bool> can_drain_to_gate(n_verts, false);
    std::queue<int> q;

    std::queue<int> gate_q;
    gate_q.push(g_apex);
    can_drain_to_gate[g_apex] = true;
    q.push(g_apex);

    while (!gate_q.empty()) {
        int curr = gate_q.front();
        gate_q.pop();

        for (int n_idx : mesh_neighbors[curr]) {
            if (!can_drain_to_gate[n_idx] && pts[n_idx].z() == z_max) {
                can_drain_to_gate[n_idx] = true;
                gate_q.push(n_idx);
                q.push(n_idx);
            }
        }
    }

    while (!q.empty()) {
        int curr = q.front();
        q.pop();

        for (int pred : ascending_predecessors[curr]) {
            if (!can_drain_to_gate[pred]) {
                can_drain_to_gate[pred] = true;
                q.push(pred);
            }
        }
    }

    // Cluster undrained vertices into connected air trap pockets
    std::vector<bool> visited(n_verts, false);
    std::vector<PeakCluster> clusters;

    for (int i = 0; i < n_verts; ++i) {
        if (can_drain_to_gate[i] || visited[i]) continue;

        // BFS over connected undrained vertices
        std::vector<int> component;
        std::queue<int> comp_q;
        comp_q.push(i);
        visited[i] = true;

        int trap_apex_idx = i;
        FT trap_max_z = pts[i].z();

        while (!comp_q.empty()) {
            int curr = comp_q.front();
            comp_q.pop();
            component.push_back(curr);

            if (pts[curr].z() > trap_max_z) {
                trap_max_z = pts[curr].z();
                trap_apex_idx = curr;
            }

            for (int n_idx : mesh_neighbors[curr]) {
                if (!can_drain_to_gate[n_idx] && !visited[n_idx]) {
                    visited[n_idx] = true;
                    comp_q.push(n_idx);
                }
            }
        }

        PeakCluster cluster;
        cluster.id = (int)clusters.size() + 1;
        cluster.apex = pts[trap_apex_idx];
        cluster.max_z = trap_max_z;
        cluster.is_primary = false;
        for (int v_idx : component) {
            cluster.vertices.push_back(ExactMesh::Vertex_index(v_idx));
        }
        clusters.push_back(cluster);
    }

    // Sort secondary trap clusters by elevation descending
    std::sort(clusters.begin(), clusters.end(), [](const PeakCluster& a, const PeakCluster& b) {
        return a.max_z > b.max_z;
    });

    // Create the primary pour gate cluster at G
    PeakCluster primary;
    primary.id = 0;
    primary.apex = pts[g_apex];
    primary.max_z = z_max;
    primary.is_primary = true;
    for (int i = 0; i < n_verts; ++i) {
        if (can_drain_to_gate[i]) {
            primary.vertices.push_back(ExactMesh::Vertex_index(i));
        }
    }

    // Primary gate cluster is always at index 0
    clusters.insert(clusters.begin(), primary);

    return clusters;
}

} // namespace pour
} // namespace geo
} // namespace jotcad
