#pragma once
#include "types.h"
#include <vector>
#include <map>

namespace jotcad {
namespace geo {
namespace mold {

// Extracts and clusters undercut faces not draft-accessible from either primary draw vector
inline std::vector<UndercutCluster> extract_undercut_clusters(
    const ExactMesh& mesh_part,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<EK::Point_3>& face_centroids,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    const EK::Vector_3& d1,
    const EK::Vector_3& d2
) {
    std::vector<ExactMesh::Face_index> face_descriptors;
    for (auto f : CGAL::faces(mesh_part)) {
        face_descriptors.push_back(f);
    }
    size_t num_faces = face_descriptors.size();
    std::vector<int> undercut_faces;
    for (size_t i = 0; i < num_faces; ++i) {
        FT dot1 = face_normals[i] * d1;
        FT dot2 = face_normals[i] * d2;
        if (dot1 <= FT(0) && dot2 <= FT(0)) {
            undercut_faces.push_back(i);
        }
    }

    if (undercut_faces.empty()) return {};

    std::map<int, int> face_to_undercut_idx;
    for (size_t i = 0; i < undercut_faces.size(); ++i) {
        face_to_undercut_idx[undercut_faces[i]] = i;
    }

    DSU dsu(undercut_faces.size());
    for (const auto& pair : edge_to_faces) {
        const auto& adj = pair.second;
        for (size_t i = 0; i < adj.size(); ++i) {
            for (size_t j = i + 1; j < adj.size(); ++j) {
                int f1 = adj[i], f2 = adj[j];
                if (face_to_undercut_idx.count(f1) && face_to_undercut_idx.count(f2)) {
                    dsu.unite(face_to_undercut_idx[f1], face_to_undercut_idx[f2]);
                }
            }
        }
    }

    std::map<int, std::vector<int>> root_to_faces;
    for (size_t i = 0; i < undercut_faces.size(); ++i) {
        root_to_faces[dsu.find(i)].push_back(undercut_faces[i]);
    }

    std::vector<UndercutCluster> clusters;
    for (const auto& pair : root_to_faces) {
        UndercutCluster c;
        c.face_indices = pair.second;
        EK::Vector_3 sum_n(0, 0, 0);
        bool first = true;
        for (int f_idx : c.face_indices) {
            sum_n = sum_n + face_normals[f_idx];
            auto f = face_descriptors[f_idx];
            auto h = mesh_part.halfedge(f);
            for (int k = 0; k < 3; ++k) {
                auto p = mesh_part.point(mesh_part.source(h));
                if (first) {
                    c.xmin = c.xmax = p.x();
                    c.ymin = c.ymax = p.y();
                    c.zmin = c.zmax = p.z();
                    first = false;
                } else {
                    c.xmin = (std::min)(c.xmin, p.x()); c.xmax = (std::max)(c.xmax, p.x());
                    c.ymin = (std::min)(c.ymin, p.y()); c.ymax = (std::max)(c.ymax, p.y());
                    c.zmin = (std::min)(c.zmin, p.z()); c.zmax = (std::max)(c.zmax, p.z());
                }
                h = mesh_part.next(h);
            }
        }
        c.avg_normal = sum_n;
        clusters.push_back(c);
    }
    return clusters;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
