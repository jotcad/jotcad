#pragma once
#include "types.h"

namespace jotcad {
namespace geo {
namespace mold {

inline std::vector<UndercutCluster> extract_undercut_clusters(
    const ExactMesh& mesh_part,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<EK::Point_3>& face_centroids,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    const EK::Vector_3& d1,
    const EK::Vector_3& d2
) {
    Tree tree(CGAL::faces(mesh_part).first, CGAL::faces(mesh_part).second, mesh_part);
    tree.build();

    // Build 1-ring topological face neighbor sets for exact primitive ID filtering
    std::vector<ExactMesh::Face_index> face_descriptors;
    for (auto f : CGAL::faces(mesh_part)) {
        face_descriptors.push_back(f);
    }

    std::map<int, std::set<ExactMesh::Face_index>> face_neighbors;
    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
        auto f = face_descriptors[f_idx];
        face_neighbors[(int)f_idx].insert(f); // Include self
        auto h = mesh_part.halfedge(f);
        for (int i = 0; i < 3; ++i) {
            auto opp = mesh_part.opposite(h);
            if (!mesh_part.is_border(opp)) {
                face_neighbors[(int)f_idx].insert(mesh_part.face(opp));
            }
            auto v = mesh_part.target(h);
            for (auto v_h : CGAL::halfedges_around_target(v, mesh_part)) {
                if (!mesh_part.is_border(v_h)) {
                    face_neighbors[(int)f_idx].insert(mesh_part.face(v_h));
                }
            }
            h = mesh_part.next(h);
        }
    }

    std::vector<int> trapped_faces;

    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
        FT dot1 = face_normals[f_idx] * d1;
        FT dot2 = face_normals[f_idx] * d2;

        bool visible_d1 = false;
        if (dot1 >= FT(0)) {
            EK::Point_3 ray_orig = face_centroids[f_idx];
            EK::Ray_3 ray(ray_orig, d1);
            std::vector<typename Tree::Intersection_and_primitive_id<EK::Ray_3>::Type> inters;
            tree.all_intersections(ray, std::back_inserter(inters));
            bool blocked = false;
            for (const auto& inter : inters) {
                if (face_neighbors[(int)f_idx].count(inter.second)) continue; // Ignore self and 1-ring neighbors
                blocked = true;
                break;
            }
            if (!blocked) visible_d1 = true;
        }

        bool visible_d2 = false;
        if (dot2 >= FT(0)) {
            EK::Point_3 ray_orig = face_centroids[f_idx];
            EK::Ray_3 ray(ray_orig, d2);
            std::vector<typename Tree::Intersection_and_primitive_id<EK::Ray_3>::Type> inters;
            tree.all_intersections(ray, std::back_inserter(inters));
            bool blocked = false;
            for (const auto& inter : inters) {
                if (face_neighbors[(int)f_idx].count(inter.second)) continue; // Ignore self and 1-ring neighbors
                blocked = true;
                break;
            }
            if (!blocked) visible_d2 = true;
        }

        if (!visible_d1 && !visible_d2) {
            trapped_faces.push_back((int)f_idx);
        }
    }

    // Spatial clustering of trapped undercut faces via adjacency DSU
    std::map<int, int> trapped_face_to_local;
    for (size_t i = 0; i < trapped_faces.size(); ++i) {
        trapped_face_to_local[trapped_faces[i]] = (int)i;
    }

    DSU cluster_dsu((int)trapped_faces.size());
    for (const auto& [edge, faces] : edge_to_faces) {
        std::vector<int> trapped_in_edge;
        for (int f : faces) {
            if (trapped_face_to_local.find(f) != trapped_face_to_local.end()) {
                trapped_in_edge.push_back(trapped_face_to_local[f]);
            }
        }
        if (trapped_in_edge.size() >= 2) {
            for (size_t i = 1; i < trapped_in_edge.size(); ++i) {
                cluster_dsu.unite(trapped_in_edge[0], trapped_in_edge[i]);
            }
        }
    }

    std::map<int, std::vector<int>> raw_clusters;
    for (size_t i = 0; i < trapped_faces.size(); ++i) {
        int root = cluster_dsu.find((int)i);
        raw_clusters[root].push_back(trapped_faces[i]);
    }

    std::vector<UndercutCluster> clusters;
    for (const auto& [root, faces] : raw_clusters) {
        UndercutCluster c;
        c.face_indices = faces;
        c.avg_normal = EK::Vector_3(FT(0), FT(0), FT(0));
        c.xmin = 1000000; c.xmax = -1000000;
        c.ymin = 1000000; c.ymax = -1000000;
        c.zmin = 1000000; c.zmax = -1000000;

        for (int f_idx : faces) {
            c.avg_normal = c.avg_normal + face_normals[f_idx];
            auto f = face_descriptors[f_idx];
            auto h = mesh_part.halfedge(f);
            auto p0 = mesh_part.point(mesh_part.source(h));
            auto p1 = mesh_part.point(mesh_part.target(h));
            auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
            std::array<EK::Point_3, 3> tri_pts = {p0, p1, p2};
            for (int i = 0; i < 3; ++i) {
                const auto& v = tri_pts[i];
                if (v.x() < c.xmin) c.xmin = v.x();
                if (v.x() > c.xmax) c.xmax = v.x();
                if (v.y() < c.ymin) c.ymin = v.y();
                if (v.y() > c.ymax) c.ymax = v.y();
                if (v.z() < c.zmin) c.zmin = v.z();
                if (v.z() > c.zmax) c.zmax = v.z();
            }
        }
        clusters.push_back(c);
    }

    return clusters;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
