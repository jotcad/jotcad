#pragma once
#include "types.h"
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

namespace jotcad {
namespace geo {
namespace mold {

// Diagnostic: Inspects all polygons touching a specific target point to detect non-manifold vertex pinches
inline void audit_vertex_umbrella(
    const std::vector<EK::Point_3>& soup_points,
    const std::vector<std::vector<size_t>>& soup_polygons,
    const EK::Point_3& target_pt,
    double tol = 1e-2
) {
    FT ft_tol = FT(tol);
    std::vector<size_t> matching_pt_indices;
    for (size_t i = 0; i < soup_points.size(); ++i) {
        if (CGAL::abs(soup_points[i].x() - target_pt.x()) < ft_tol &&
            CGAL::abs(soup_points[i].y() - target_pt.y()) < ft_tol &&
            CGAL::abs(soup_points[i].z() - target_pt.z()) < ft_tol) {
            matching_pt_indices.push_back(i);
        }
    }

    if (matching_pt_indices.empty()) return;

    std::cout << "    [Umbrella Audit] Target Point ("
              << CGAL::to_double(target_pt.x()) << ", "
              << CGAL::to_double(target_pt.y()) << ", "
              << CGAL::to_double(target_pt.z()) << ") matches "
              << matching_pt_indices.size() << " soup point descriptors:" << std::endl;

    std::set<size_t> match_set(matching_pt_indices.begin(), matching_pt_indices.end());
    std::vector<size_t> incident_polys;
    for (size_t poly_idx = 0; poly_idx < soup_polygons.size(); ++poly_idx) {
        for (size_t v : soup_polygons[poly_idx]) {
            if (match_set.count(v)) {
                incident_polys.push_back(poly_idx);
                break;
            }
        }
    }

    std::cout << "    [Umbrella Audit] Found " << incident_polys.size() << " incident polygons in raw soup:" << std::endl;
    for (size_t poly_idx : incident_polys) {
        const auto& poly = soup_polygons[poly_idx];
        std::cout << "      poly #" << poly_idx << ": ";
        for (size_t v_idx : poly) {
            std::cout << "v" << v_idx << "("
                      << CGAL::to_double(soup_points[v_idx].x()) << ", "
                      << CGAL::to_double(soup_points[v_idx].y()) << ", "
                      << CGAL::to_double(soup_points[v_idx].z()) << ") ";
        }
        std::cout << std::endl;
    }
}

// Diagnostic: Audits polygon soup edge orientation consistency
inline void audit_polygon_soup(
    const std::vector<EK::Point_3>& soup_points,
    const std::vector<std::vector<size_t>>& soup_polygons
) {
    std::map<std::pair<size_t, size_t>, int> directed_edge_counts;
    for (const auto& poly : soup_polygons) {
        for (size_t i = 0; i < poly.size(); ++i) {
            size_t u = poly[i];
            size_t v = poly[(i + 1) % poly.size()];
            directed_edge_counts[{u, v}]++;
        }
    }

    int duplicated_halfedges = 0;
    int single_halfedges = 0;
    int matched_halfedges = 0;

    std::set<std::pair<size_t, size_t>> checked_undirected;
    for (const auto& [edge, count] : directed_edge_counts) {
        size_t u = edge.first;
        size_t v = edge.second;
        auto undirected = std::make_pair((std::min)(u, v), (std::max)(u, v));
        if (checked_undirected.count(undirected)) continue;
        checked_undirected.insert(undirected);

        int fwd = count;
        int bwd = directed_edge_counts.count({v, u}) ? directed_edge_counts.at({v, u}) : 0;

        if (fwd > 1 || bwd > 1) duplicated_halfedges++;
        if (fwd + bwd == 1) single_halfedges++;
        if (fwd == 1 && bwd == 1) matched_halfedges++;
    }

    std::cout << "    [Soup Adjacency Audit] Matched 2-manifold edges: " << matched_halfedges
              << " | Unmatched open gaps: " << single_halfedges
              << " | Inconsistent duplicate halfedges: " << duplicated_halfedges << std::endl;
}

// Diagnostic: Inspects self-intersecting face pairs in exact mesh
inline void inspect_self_intersections(
    const ExactMesh& mesh,
    const CGAL::Aff_transformation_3<EK>& to_z
) {
    std::vector<std::pair<ExactMesh::Face_index, ExactMesh::Face_index>> intersected_pairs;
    CGAL::Polygon_mesh_processing::self_intersections(mesh, std::back_inserter(intersected_pairs));
    if (!intersected_pairs.empty()) {
        std::cout << "    [Self-Intersections] Total intersecting face pairs: " << intersected_pairs.size() << std::endl;
        for (size_t i = 0; i < (std::min)(intersected_pairs.size(), size_t(3)); ++i) {
            auto f1 = intersected_pairs[i].first;
            auto f2 = intersected_pairs[i].second;
            std::cout << "      Pair #" << (i + 1) << ": Face " << f1 << " vs Face " << f2 << std::endl;

            std::vector<ExactMesh::Vertex_index> v_f1, v_f2;
            auto get_verts = [&](ExactMesh::Face_index f, std::vector<ExactMesh::Vertex_index>& out_v, const char* name) {
                std::cout << "        " << name << " " << f << ": ";
                auto h = mesh.halfedge(f);
                auto curr = h;
                do {
                    auto v = mesh.target(curr);
                    out_v.push_back(v);
                    auto p = mesh.point(v);
                    auto pr = to_z(p);
                    std::cout << "v" << v.idx() << "(X=" << CGAL::to_double(pr.x()) << ", Y=" << CGAL::to_double(pr.y()) << ", Z=" << CGAL::to_double(pr.z()) << ") ";
                    curr = mesh.next(curr);
                } while (curr != h);
                std::cout << std::endl;
            };
            get_verts(f1, v_f1, "f1");
            get_verts(f2, v_f2, "f2");

            for (auto va : v_f1) {
                auto pa = to_z(mesh.point(va));
                for (auto vb : v_f2) {
                    auto pb = to_z(mesh.point(vb));
                    FT dx = pa.x() - pb.x();
                    FT dy = pa.y() - pb.y();
                    FT dz = pa.z() - pb.z();
                    if (CGAL::abs(dx) < FT(1e-4) && CGAL::abs(dy) < FT(1e-4)) {
                        std::cout << "        [Vertex Comparison] v" << va.idx() << " vs v" << vb.idx() << ":" << std::endl;
                        std::cout << "          dx = " << CGAL::to_double(dx) << " (is_zero: " << (dx == FT(0)) << ")" << std::endl;
                        std::cout << "          dy = " << CGAL::to_double(dy) << " (is_zero: " << (dy == FT(0)) << ")" << std::endl;
                        std::cout << "          dz = " << CGAL::to_double(dz) << " (is_zero: " << (dz == FT(0)) << ")" << std::endl;
                    }
                }
            }
        }
    }
}

} // namespace mold
} // namespace geo
} // namespace jotcad
