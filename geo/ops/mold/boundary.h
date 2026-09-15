#pragma once
#include "types.h"
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <map>
#include <vector>
#include <set>
#include <iostream>

namespace jotcad {
namespace geo {
namespace mold {

typedef CGAL::Polygon_2<EK> Polygon_2;

struct BoundarySimplicityResult {
    bool is_simple = true;
    bool has_pinch = false;
    size_t loop_count = 0;
    std::vector<size_t> loop_vertex_counts;
    std::vector<bool> loop_is_simple;
};

/**
 * @brief Chains 2D directed segments into closed loops and audits them for simplicity.
 */
inline BoundarySimplicityResult audit_2d_boundary_simplicity(
    const std::vector<std::pair<EK::Point_2, EK::Point_2>>& segments,
    const std::string& label = "Boundary"
) {
    BoundarySimplicityResult result;
    if (segments.empty()) return result;

    std::map<EK::Point_2, std::vector<EK::Point_2>> next_map;
    std::map<EK::Point_2, int> in_degree;

    for (const auto& [a, b] : segments) {
        if (a == b) continue;
        next_map[a].push_back(b);
        in_degree[b]++;
    }

    for (const auto& [pt, out_edges] : next_map) {
        if (out_edges.size() > 1 || in_degree[pt] > 1) {
            result.has_pinch = true;
            result.is_simple = false;
        }
    }

    std::set<EK::Point_2> visited;
    for (const auto& [start_pt, _] : segments) {
        if (visited.count(start_pt)) continue;

        Polygon_2 poly;
        auto curr = start_pt;
        bool closed = false;

        while (!visited.count(curr)) {
            visited.insert(curr);
            poly.push_back(curr);

            auto it = next_map.find(curr);
            if (it == next_map.end() || it->second.empty()) break;
            curr = it->second[0];
            if (curr == start_pt) {
                closed = true;
                break;
            }
        }

        if (closed && poly.size() >= 3) {
            result.loop_count++;
            result.loop_vertex_counts.push_back(poly.size());
            bool loop_simple = poly.is_simple();
            result.loop_is_simple.push_back(loop_simple);
            if (!loop_simple) {
                result.is_simple = false;
            }
        }
    }

    std::cout << "    [" << label << " Simplicity Audit] Loops: " << result.loop_count
              << " | has_pinch: " << (result.has_pinch ? "YES" : "NO")
              << " | overall is_simple: " << (result.is_simple ? "YES" : "NO");
    for (size_t i = 0; i < result.loop_vertex_counts.size(); ++i) {
        std::cout << " [Loop " << (i + 1) << ": " << result.loop_vertex_counts[i] 
                  << " vtx, simple=" << (result.loop_is_simple[i] ? "YES" : "NO") << "]";
    }
    std::cout << std::endl << std::flush;

    return result;
}

/**
 * @brief Projects 3D mesh patch border loops onto the 2D plane orthogonal to draw_dir and audits simplicity.
 */
inline BoundarySimplicityResult audit_patch_projected_boundary_simplicity(
    const ExactMesh& mesh,
    const std::vector<ExactMesh::Face_index>& patch_faces,
    const EK::Vector_3& draw_dir,
    const std::string& label = "Patch Projected Boundary"
) {
    BoundarySimplicityResult result;
    if (patch_faces.empty()) return result;

    // Pure FT tangent basis
    EK::FT dx = draw_dir.x(), dy = draw_dir.y(), dz = draw_dir.z();
    EK::FT abs_dx = (dx < FT(0)) ? -dx : dx;
    EK::FT abs_dy = (dy < FT(0)) ? -dy : dy;
    EK::FT abs_dz = (dz < FT(0)) ? -dz : dz;
    EK::Vector_3 ref(FT(0), FT(0), FT(0));
    if (abs_dx <= abs_dy && abs_dx <= abs_dz) ref = EK::Vector_3(FT(1), FT(0), FT(0));
    else if (abs_dy <= abs_dz) ref = EK::Vector_3(FT(0), FT(1), FT(0));
    else ref = EK::Vector_3(FT(0), FT(0), FT(1));

    EK::Vector_3 u_basis = CGAL::cross_product(draw_dir, ref);
    EK::Vector_3 v_basis = CGAL::cross_product(draw_dir, u_basis);

    std::vector<ExactMesh::Halfedge_index> border_hes;
    CGAL::Polygon_mesh_processing::border_halfedges(patch_faces, mesh, std::back_inserter(border_hes));

    std::map<int, int> next_v;
    for (auto h : border_hes) {
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        next_v[u] = v;
    }

    std::set<int> visited;
    for (auto h : border_hes) {
        int start = (int)mesh.source(h);
        if (visited.count(start)) continue;

        std::vector<int> v_chain;
        int curr = start;
        while (curr != -1 && !visited.count(curr)) {
            visited.insert(curr);
            v_chain.push_back(curr);
            auto it = next_v.find(curr);
            if (it == next_v.end()) break;
            curr = it->second;
            if (curr == start) break;
        }

        if (curr == start && v_chain.size() >= 3) {
            Polygon_2 poly;
            for (int v_idx : v_chain) {
                const auto& p = mesh.point(ExactMesh::Vertex_index(v_idx));
                EK::FT up = p.x()*u_basis.x() + p.y()*u_basis.y() + p.z()*u_basis.z();
                EK::FT vp = p.x()*v_basis.x() + p.y()*v_basis.y() + p.z()*v_basis.z();
                EK::Point_2 pt2d(up, vp);
                if (poly.is_empty() || poly.vertices().back() != pt2d) {
                    poly.push_back(pt2d);
                }
            }
            if (poly.size() >= 3 && poly.vertices().front() == poly.vertices().back()) {
                poly.erase(poly.vertices_end() - 1);
            }

            if (poly.size() >= 3) {
                result.loop_count++;
                result.loop_vertex_counts.push_back(poly.size());
                bool simple = poly.is_simple();
                result.loop_is_simple.push_back(simple);
                if (!simple) result.is_simple = false;
            }
        }
    }

    std::cout << "    [" << label << " Simplicity Audit] Loops: " << result.loop_count
              << " | overall is_simple: " << (result.is_simple ? "YES" : "NO");
    for (size_t i = 0; i < result.loop_vertex_counts.size(); ++i) {
        std::cout << " [Loop " << (i + 1) << ": " << result.loop_vertex_counts[i] 
                  << " vtx, simple=" << (result.loop_is_simple[i] ? "YES" : "NO") << "]";
    }
    std::cout << std::endl << std::flush;

    return result;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
