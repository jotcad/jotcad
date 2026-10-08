#pragma once

#include "types.h"
#include "envelope.h"
#include <vector>
#include <map>
#include <set>
#include <CGAL/Polygon_mesh_processing/border.h>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Computes an exact rational tangent basis (u, v) orthogonal to vector d.
 * 
 * Uses pure EK::FT arithmetic to construct a right-handed orthogonal frame
 * where u . d == 0, v . d == 0, and u . v == 0.
 */
inline std::pair<EK::Vector_3, EK::Vector_3> compute_exact_tangent_basis(const EK::Vector_3& d) {
    EK::FT dx = d.x();
    EK::FT dy = d.y();
    EK::FT dz = d.z();
    EK::FT abs_dx = (dx < EK::FT(0)) ? -dx : dx;
    EK::FT abs_dy = (dy < EK::FT(0)) ? -dy : dy;
    EK::FT abs_dz = (dz < EK::FT(0)) ? -dz : dz;

    EK::Vector_3 ref(EK::FT(0), EK::FT(0), EK::FT(0));
    if (abs_dx <= abs_dy && abs_dx <= abs_dz) {
        ref = EK::Vector_3(EK::FT(1), EK::FT(0), EK::FT(0));
    } else if (abs_dy <= abs_dz) {
        ref = EK::Vector_3(EK::FT(0), EK::FT(1), EK::FT(0));
    } else {
        ref = EK::Vector_3(EK::FT(0), EK::FT(0), EK::FT(1));
    }

    EK::Vector_3 u = CGAL::cross_product(d, ref);
    EK::Vector_3 v = CGAL::cross_product(d, u);
    return {u, v};
}

/**
 * @brief 2D exact bounding box in the tangent plane (u, v).
 */
struct BoundingBox2D {
    EK::FT u_min = 0, u_max = 0;
    EK::FT v_min = 0, v_max = 0;

    bool overlaps(const BoundingBox2D& other) const {
        if (u_max < other.u_min || other.u_max < u_min) return false;
        if (v_max < other.v_min || other.v_max < v_min) return false;
        return true;
    }
};

/**
 * @brief Counts the number of independent exterior/interior boundary cycles for a component.
 * 
 * If cycle_count == 1: the component is a topological disk (zero internal undercut holes).
 * If cycle_count > 1: the component contains interior holes / undercut islands.
 */
inline int count_component_boundary_cycles(
    const std::vector<ExactMesh::Face_index>& comp_faces,
    const ExactMesh& mesh_part
) {
    std::vector<ExactMesh::Halfedge_index> border_halfedges;
    CGAL::Polygon_mesh_processing::border_halfedges(
        comp_faces, mesh_part, std::back_inserter(border_halfedges)
    );

    std::map<int, int> next_v;
    for (auto h : border_halfedges) {
        int u = (int)mesh_part.source(h);
        int v = (int)mesh_part.target(h);
        next_v[u] = v;
    }

    std::set<int> visited;
    int cycle_count = 0;
    for (auto h : border_halfedges) {
        int start = (int)mesh_part.source(h);
        if (visited.count(start)) continue;

        int curr = start;
        int step = 0;
        while (curr != -1 && !visited.count(curr)) {
            visited.insert(curr);
            auto it = next_v.find(curr);
            if (it == next_v.end()) break;
            int nxt = it->second;
            step++;
            curr = nxt;
            if (curr == start) break;
        }
        if (curr == start && step >= 3) {
            cycle_count++;
        }
    }
    return cycle_count;
}

/**
 * @brief Result of candidate patch extraction and multi-component aggregation.
 */
struct CandidatePatch {
    std::vector<ExactMesh::Face_index> faces;
    int cycle_count = 0;
    bool is_valid = false;
    FT total_area = FT(0);
};

/**
 * @brief Extracts visible positive-draft faces along vector d and aggregates non-overlapping components.
 * 
 * 1. Filters unhandled faces with draft (n_f . d >= min_dot).
 * 2. Groups them into connected components via edge_to_faces adjacency.
 * 3. Audits each component for disk topology (cycle_count == 1).
 * 4. Aggregates non-overlapping disk components in projected tangent space (u, v).
 */
template <typename FaceHandledMap = FaceBoolMap>
inline CandidatePatch extract_candidate_patch(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceHandledMap is_handled,
    const EK::Vector_3& d,
    const FT& min_dot = FT(0)
) {
    auto visible_faces = compute_visible_patch_faces_fast(
        mesh_part, face_descriptors, face_normals, is_handled, d, min_dot
    );
    if (visible_faces.empty()) {
        return {{}, 0, false, FT(0)};
    }

    // Map visible faces to local indices for DSU
    std::map<ExactMesh::Face_index, int> face_to_local;
    for (size_t i = 0; i < visible_faces.size(); ++i) {
        face_to_local[visible_faces[i]] = (int)i;
    }

    DSU patch_dsu((int)visible_faces.size());
    for (const auto& [edge, faces] : edge_to_faces) {
        std::vector<int> visible_in_edge;
        for (int f_idx : faces) {
            auto f = ExactMesh::Face_index(f_idx);
            auto it = face_to_local.find(f);
            if (it != face_to_local.end()) {
                visible_in_edge.push_back(it->second);
            }
        }
        if (visible_in_edge.size() >= 2) {
            for (size_t i = 1; i < visible_in_edge.size(); ++i) {
                patch_dsu.unite(visible_in_edge[0], visible_in_edge[i]);
            }
        }
    }

    std::map<int, std::vector<ExactMesh::Face_index>> components;
    for (size_t i = 0; i < visible_faces.size(); ++i) {
        int root = patch_dsu.find((int)i);
        components[root].push_back(visible_faces[i]);
    }

    auto [u_basis, v_basis] = compute_exact_tangent_basis(d);

    struct CompData {
        std::vector<ExactMesh::Face_index> faces;
        FT area = FT(0);
        int cycle_count = 0;
        BoundingBox2D bbox;
    };

    std::vector<CompData> all_comps;

    for (const auto& [root, comp_faces] : components) {
        FT comp_area = FT(0);
        BoundingBox2D bbox;
        bool first_pt = true;

        for (auto f : comp_faces) {
            size_t f_idx = (size_t)f;
            comp_area += face_areas[f_idx];

            auto h = mesh_part.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                const auto& p = mesh_part.point(mesh_part.target(h));
                FT up = p.x()*u_basis.x() + p.y()*u_basis.y() + p.z()*u_basis.z();
                FT vp = p.x()*v_basis.x() + p.y()*v_basis.y() + p.z()*v_basis.z();
                if (first_pt) {
                    bbox.u_min = bbox.u_max = up;
                    bbox.v_min = bbox.v_max = vp;
                    first_pt = false;
                } else {
                    if (up < bbox.u_min) bbox.u_min = up;
                    if (up > bbox.u_max) bbox.u_max = up;
                    if (vp < bbox.v_min) bbox.v_min = vp;
                    if (vp > bbox.v_max) bbox.v_max = vp;
                }
                h = mesh_part.next(h);
            }
        }

        if (comp_area <= FT(0)) continue;

        int cycles = count_component_boundary_cycles(comp_faces, mesh_part);
        all_comps.push_back(CompData{comp_faces, comp_area, cycles, bbox});
    }

    if (all_comps.empty()) {
        return {{}, 0, false, FT(0)};
    }

    std::sort(all_comps.begin(), all_comps.end(), [](const auto& a, const auto& b) {
        return a.area > b.area;
    });

    std::vector<BoundingBox2D> accepted_bboxes;
    std::vector<ExactMesh::Face_index> accepted_faces = all_comps[0].faces;
    FT total_accepted_area = all_comps[0].area;
    accepted_bboxes.push_back(all_comps[0].bbox);

    for (size_t i = 1; i < all_comps.size(); ++i) {
        const auto& cand = all_comps[i];
        bool overlaps = false;
        for (const auto& acc_box : accepted_bboxes) {
            if (cand.bbox.overlaps(acc_box)) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            accepted_faces.insert(accepted_faces.end(), cand.faces.begin(), cand.faces.end());
            total_accepted_area += cand.area;
            accepted_bboxes.push_back(cand.bbox);
        }
    }

    return {accepted_faces, (int)accepted_bboxes.size(), true, total_accepted_area};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
