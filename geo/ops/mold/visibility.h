#pragma once
#include "types.h"
#include <CGAL/Env_triangle_traits_3.h>
#include <CGAL/Env_surface_data_traits_3.h>
#include <CGAL/envelope_3.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <list>

namespace jotcad {
namespace geo {
namespace mold {

typedef CGAL::Env_triangle_traits_3<EK> Env_traits_3;
typedef CGAL::Env_surface_data_traits_3<Env_traits_3, size_t> Data_traits_3;
typedef Data_traits_3::Surface_3 Data_triangle_3;
typedef CGAL::Envelope_diagram_2<Data_traits_3> Envelope_diagram_2;

inline std::vector<ExactMesh::Face_index> compute_visible_patch_faces_fast(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    FaceBoolMap is_handled,
    const EK::Vector_3& d,
    FT min_dot
) {
    std::vector<ExactMesh::Face_index> visible_faces;
    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
        auto f = face_descriptors[f_idx];
        if (is_handled[f]) continue;
        if (face_normals[f_idx] * d >= min_dot) {
            visible_faces.push_back(f);
        }
    }
    return visible_faces;
}

struct EnvelopeMeshResult {
    ExactMesh solid_wedge;
    std::set<size_t> source_faces;
    FT total_area;
};

inline EnvelopeMeshResult compute_exact_upper_envelope_mesh(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    FaceBoolMap is_handled,
    const EK::Vector_3& d,
    FT min_dot,
    const std::vector<ExactMesh::Face_index>& seed_patch_faces = {}
) {
    // Pure FT exact orthogonal frame
    FT dx = d.x(), dy = d.y(), dz = d.z();
    FT d_sq = dx*dx + dy*dy + dz*dz;

    // Choose reference u not collinear with d in pure FT
    FT ux = 0, uy = 0, uz = 1;
    if (dz * dz * FT(10) > d_sq * FT(9)) {
        ux = 1; uy = 0; uz = 0;
    }

    FT u_dot_d = ux*dx + uy*dy + uz*dz;
    // x_basis = u - (u . d / d_sq) * d
    FT xx = ux - (u_dot_d * dx) / d_sq;
    FT xy = uy - (u_dot_d * dy) / d_sq;
    FT xz = uz - (u_dot_d * dz) / d_sq;
    FT x_sq = xx*xx + xy*xy + xz*xz;

    // y_basis = d x x_basis
    FT yx = dy*xz - dz*xy;
    FT yy = dz*xx - dx*xz;
    FT yz = dx*xy - dy*xx;
    FT y_sq = yx*yx + yy*yy + yz*yz;

    auto rotate_pt = [&](const EK::Point_3& p) -> EK::Point_3 {
        FT px = p.x(), py = p.y(), pz = p.z();
        FT rx = px*xx + py*xy + pz*xz;
        FT ry = px*yx + py*yy + pz*yz;
        FT rz = px*dx + py*dy + pz*dz;
        return EK::Point_3(rx, ry, rz);
    };

    auto unrotate_pt = [&](FT rx, FT ry, FT rz) -> EK::Point_3 {
        FT cx = rx / x_sq;
        FT cy = ry / y_sq;
        FT cz = rz / d_sq;
        FT px = cx*xx + cy*yx + cz*dx;
        FT py = cx*xy + cy*yy + cz*dy;
        FT pz = cx*xz + cy*yz + cz*dz;
        return EK::Point_3(px, py, pz);
    };

    FT p_xmin, p_xmax, p_ymin, p_ymax, p_zmin;
    bool has_seed = !seed_patch_faces.empty();
    if (has_seed) {
        bool first = true;
        for (auto f : seed_patch_faces) {
            auto h = mesh_part.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                auto p_rot = rotate_pt(mesh_part.point(mesh_part.source(h)));
                if (first) {
                    p_xmin = p_xmax = p_rot.x();
                    p_ymin = p_ymax = p_rot.y();
                    p_zmin = p_rot.z();
                    first = false;
                } else {
                    p_xmin = (std::min)(p_xmin, p_rot.x());
                    p_xmax = (std::max)(p_xmax, p_rot.x());
                    p_ymin = (std::min)(p_ymin, p_rot.y());
                    p_ymax = (std::max)(p_ymax, p_rot.y());
                    p_zmin = (std::min)(p_zmin, p_rot.z());
                }
                h = mesh_part.next(h);
            }
        }
    }

    std::map<size_t, std::array<EK::Point_3, 3>> rotated_tris;
    std::list<Data_triangle_3> triangles;
    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
        auto f = face_descriptors[f_idx];
        if (is_handled[f]) continue;
        if (face_normals[f_idx] * d < min_dot) continue;

        auto h = mesh_part.halfedge(f);
        auto p0 = rotate_pt(mesh_part.point(mesh_part.source(h)));
        auto p1 = rotate_pt(mesh_part.point(mesh_part.target(h)));
        auto p2 = rotate_pt(mesh_part.point(mesh_part.target(mesh_part.next(h))));

        if (has_seed) {
            FT f_xmin = (std::min)({p0.x(), p1.x(), p2.x()});
            FT f_xmax = (std::max)({p0.x(), p1.x(), p2.x()});
            FT f_ymin = (std::min)({p0.y(), p1.y(), p2.y()});
            FT f_ymax = (std::max)({p0.y(), p1.y(), p2.y()});
            FT f_zmax = (std::max)({p0.z(), p1.z(), p2.z()});

            if (f_xmax < p_xmin || f_xmin > p_xmax || f_ymax < p_ymin || f_ymin > p_ymax || f_zmax < p_zmin) {
                continue;
            }
        }

        if (!CGAL::collinear(p0, p1, p2)) {
            EK::Triangle_3 tri(p0, p1, p2);
            triangles.push_back(Data_triangle_3(tri, f_idx));
            rotated_tris[f_idx] = {p0, p1, p2};
        }
    }

    if (triangles.empty()) return {};

    std::cout << "    [Envelope] OBB Corridor filtered: " << triangles.size() << " / " << face_descriptors.size() << " candidate triangles." << std::endl << std::flush;
    std::cout << "    [Envelope] Computing CGAL::upper_envelope_3 on " << triangles.size() << " triangles..." << std::flush;
    auto t_env_start = std::chrono::steady_clock::now();

    Envelope_diagram_2 max_diag;
    CGAL::upper_envelope_3(triangles.begin(), triangles.end(), max_diag);

    auto t_env_end = std::chrono::steady_clock::now();
    double env_ms = std::chrono::duration<double, std::milli>(t_env_end - t_env_start).count();
    std::cout << " Done in " << env_ms << "ms (faces in diagram: " << max_diag.number_of_faces() << ")." << std::endl << std::flush;

    auto get_z = [&](size_t orig_f_idx, FT vx, FT vy) -> FT {
        const auto& tri_pts = rotated_tris[orig_f_idx];
        const auto& p0 = tri_pts[0];
        const auto& p1 = tri_pts[1];
        const auto& p2 = tri_pts[2];
        EK::Vector_3 n = CGAL::cross_product(p1 - p0, p2 - p0);
        if (n.z() == FT(0)) return p0.z();
        return p0.z() - (n.x() * (vx - p0.x()) + n.y() * (vy - p0.y())) / n.z();
    };

    std::vector<EK::Point_3> soup_points;
    std::vector<std::vector<size_t>> soup_polygons;
    std::set<size_t> source_faces;

    FT max_vz_rot = -1000000;
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f_idx = fit->surfaces_begin()->data();
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            auto p2d = curr->target()->point();
            FT vz = get_z(orig_f_idx, p2d.x(), p2d.y());
            if (vz > max_vz_rot) max_vz_rot = vz;
            curr = curr->next();
        } while (curr != ccb);
    }

    FT h_ceiling_rot = max_vz_rot + FT(50);

    // 1. Add all illuminated surface cells (floor + symmetrical ceiling)
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;

        size_t orig_f_idx = fit->surfaces_begin()->data();
        source_faces.insert(orig_f_idx);

        std::vector<size_t> bot_indices;
        std::vector<EK::Point_2> cell_pts_2d;
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            auto p2d = curr->target()->point();
            cell_pts_2d.push_back(p2d);
            FT vx = p2d.x();
            FT vy = p2d.y();
            FT vz = get_z(orig_f_idx, vx, vy);

            EK::Point_3 world_p = unrotate_pt(vx, vy, vz);
            size_t idx = soup_points.size();
            soup_points.push_back(world_p);
            bot_indices.push_back(idx);
            curr = curr->next();
        } while (curr != ccb);

        if (bot_indices.size() >= 3) {
            soup_polygons.push_back(bot_indices);
        }

        // Symmetrical ceiling cap for this cell (reversed winding so outward normal points +Z)
        std::vector<size_t> top_indices;
        for (int i = (int)cell_pts_2d.size() - 1; i >= 0; --i) {
            EK::Point_3 top_p = unrotate_pt(cell_pts_2d[i].x(), cell_pts_2d[i].y(), h_ceiling_rot);
            size_t idx = soup_points.size();
            soup_points.push_back(top_p);
            top_indices.push_back(idx);
        }
        if (top_indices.size() >= 3) {
            soup_polygons.push_back(top_indices);
        }
    }

    // Precompute all active distinct surface heights at each arrangement vertex
    std::map<Envelope_diagram_2::Vertex_handle, std::set<FT>> vertex_heights;
    for (auto vit = max_diag.vertices_begin(); vit != max_diag.vertices_end(); ++vit) {
        vertex_heights[vit].insert(h_ceiling_rot);
        auto e_curr = vit->incident_halfedges();
        auto e_start = e_curr;
        do {
            auto f1 = e_curr->face();
            if (!f1->is_unbounded() && f1->number_of_surfaces() > 0) {
                size_t orig_f = f1->surfaces_begin()->data();
                FT z = get_z(orig_f, vit->point().x(), vit->point().y());
                vertex_heights[vit].insert(z);
            }
            ++e_curr;
        } while (e_curr != e_start);
    }

    // 2. Add vertical cliff quads at step discontinuities with intermediate height subdivision
    for (auto eit = max_diag.edges_begin(); eit != max_diag.edges_end(); ++eit) {
        auto f1 = eit->face();
        auto f2 = eit->twin()->face();
        if (f1->is_unbounded() || f2->is_unbounded()) continue;
        if (f1->number_of_surfaces() == 0 || f2->number_of_surfaces() == 0) continue;

        size_t orig_f1 = f1->surfaces_begin()->data();
        size_t orig_f2 = f2->surfaces_begin()->data();
        if (orig_f1 == orig_f2) continue;

        auto p1_2d = eit->source()->point();
        auto p2_2d = eit->target()->point();

        FT z1_s = get_z(orig_f1, p1_2d.x(), p1_2d.y());
        FT z1_t = get_z(orig_f1, p2_2d.x(), p2_2d.y());

        FT z2_s = get_z(orig_f2, p1_2d.x(), p1_2d.y());
        FT z2_t = get_z(orig_f2, p2_2d.x(), p2_2d.y());

        if (z1_s != z2_s || z1_t != z2_t) {
            bool f1_is_higher = ((z1_s + z1_t) >= (z2_s + z2_t));
            FT low_s = f1_is_higher ? z2_s : z1_s;
            FT high_s = f1_is_higher ? z1_s : z2_s;
            FT low_t = f1_is_higher ? z2_t : z1_t;
            FT high_t = f1_is_higher ? z1_t : z2_t;

            std::vector<FT> right_zs;
            for (FT z : vertex_heights[eit->target()]) {
                if (z >= low_t && z <= high_t) right_zs.push_back(z);
            }
            std::sort(right_zs.begin(), right_zs.end());

            std::vector<FT> left_zs;
            for (FT z : vertex_heights[eit->source()]) {
                if (z >= low_s && z <= high_s) left_zs.push_back(z);
            }
            std::sort(left_zs.begin(), left_zs.end(), std::greater<FT>());

            std::vector<size_t> poly_idxs;

            // Bottom left
            poly_idxs.push_back(soup_points.size());
            soup_points.push_back(unrotate_pt(p1_2d.x(), p1_2d.y(), low_s));

            // Up right vertical edge
            for (FT z : right_zs) {
                poly_idxs.push_back(soup_points.size());
                soup_points.push_back(unrotate_pt(p2_2d.x(), p2_2d.y(), z));
            }

            // Down left vertical edge
            for (FT z : left_zs) {
                if (z == low_s) continue;
                poly_idxs.push_back(soup_points.size());
                soup_points.push_back(unrotate_pt(p1_2d.x(), p1_2d.y(), z));
            }

            if (poly_idxs.size() >= 3) {
                soup_polygons.push_back(poly_idxs);
            }
        }
    }

    // 3. Add swept sidewall quads for true outer/aperture boundary halfedges (adjacent to unbounded space or empty cells)

    auto add_sidewall_if_boundary = [&](Envelope_diagram_2::Halfedge_handle h, size_t orig_f) {
        if (h->twin()->face()->is_unbounded() || h->twin()->face()->number_of_surfaces() == 0) {
            auto p1_2d = h->source()->point();
            auto p2_2d = h->target()->point();

            FT z_s = get_z(orig_f, p1_2d.x(), p1_2d.y());
            FT z_t = get_z(orig_f, p2_2d.x(), p2_2d.y());

            std::vector<FT> right_zs;
            for (FT z : vertex_heights[h->target()]) {
                if (z >= z_t && z <= h_ceiling_rot) right_zs.push_back(z);
            }
            std::sort(right_zs.begin(), right_zs.end());

            std::vector<FT> left_zs;
            for (FT z : vertex_heights[h->source()]) {
                if (z >= z_s && z <= h_ceiling_rot) left_zs.push_back(z);
            }
            std::sort(left_zs.begin(), left_zs.end());

            std::vector<size_t> poly_idxs;

            // 1. Bottom right (p2, z_t)
            poly_idxs.push_back(soup_points.size());
            soup_points.push_back(unrotate_pt(p2_2d.x(), p2_2d.y(), z_t));

            // 2. Up left vertical edge from (p1, z_s) to (p1, h_ceiling)
            for (FT z : left_zs) {
                poly_idxs.push_back(soup_points.size());
                soup_points.push_back(unrotate_pt(p1_2d.x(), p1_2d.y(), z));
            }

            // 3. Down right vertical edge from (p2, h_ceiling) to just above z_t
            for (int i = (int)right_zs.size() - 1; i >= 0; --i) {
                if (right_zs[i] == z_t) continue;
                poly_idxs.push_back(soup_points.size());
                soup_points.push_back(unrotate_pt(p2_2d.x(), p2_2d.y(), right_zs[i]));
            }

            if (poly_idxs.size() >= 3) {
                soup_polygons.push_back(poly_idxs);
            }
        }
    };

    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f = fit->surfaces_begin()->data();

        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            add_sidewall_if_boundary(curr, orig_f);
            curr = curr->next();
        } while (curr != ccb);

        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            do {
                add_sidewall_if_boundary(h_curr, orig_f);
                h_curr = h_curr->next();
            } while (h_curr != h_start);
        }
    }

    if (soup_polygons.empty()) return {};

    std::cout << "    [Envelope] Polygon soup has " << soup_polygons.size() << " polygons (" << soup_points.size() << " points). Repairing soup..." << std::flush;
    auto t_soup_start = std::chrono::steady_clock::now();

    CGAL::Polygon_mesh_processing::repair_polygon_soup(soup_points, soup_polygons);
    std::cout << " Done. Orienting soup (" << soup_polygons.size() << " polygons, " << soup_points.size() << " points)..." << std::flush;

    CGAL::Polygon_mesh_processing::orient_polygon_soup(soup_points, soup_polygons);
    std::cout << " Done. Converting to mesh..." << std::flush;

    ExactMesh solid_wedge;
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(soup_points, soup_polygons, solid_wedge);
    CGAL::Polygon_mesh_processing::stitch_borders(solid_wedge);
    CGAL::Polygon_mesh_processing::triangulate_faces(solid_wedge);
    solid_wedge.collect_garbage();

    if (CGAL::is_closed(solid_wedge)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(solid_wedge);
    }

    auto t_soup_end = std::chrono::steady_clock::now();
    double soup_ms = std::chrono::duration<double, std::milli>(t_soup_end - t_soup_start).count();
    std::cout << " Done in " << soup_ms << "ms." << std::endl << std::flush;

    bool is_closed = CGAL::is_closed(solid_wedge);
    bool self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(solid_wedge);
    std::cout << "  [Wedge Validation] is_closed: " << (is_closed ? "YES" : "NO")
              << " | does_self_intersect: " << (self_intersects ? "YES" : "NO")
              << " | vertices: " << solid_wedge.number_of_vertices()
              << " | faces: " << solid_wedge.number_of_faces() << std::endl << std::flush;

    // 1. Direct Edge Direction Consistency Check on Polygon Soup
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
        if (fwd + bwd == 1) {
            single_halfedges++;
            if (single_halfedges <= 10) {
                auto p0 = soup_points[u];
                auto p1 = soup_points[v];
                auto pr0 = rotate_pt(p0);
                auto pr1 = rotate_pt(p1);
                std::cout << "      [Open Gap #" << single_halfedges << "] 2D: ("
                          << CGAL::to_double(pr0.x()) << ", " << CGAL::to_double(pr0.y()) << ") -> ("
                          << CGAL::to_double(pr1.x()) << ", " << CGAL::to_double(pr1.y()) << ")"
                          << " | Z: " << CGAL::to_double(pr0.z()) << " -> " << CGAL::to_double(pr1.z())
                          << " (h_ceiling=" << CGAL::to_double(h_ceiling_rot) << ")"
                          << std::endl;
            }
        }
        if (fwd == 1 && bwd == 1) matched_halfedges++;
    }

    std::cout << "    [Soup Adjacency Audit] Matched 2-manifold edges: " << matched_halfedges
              << " | Unmatched open gaps: " << single_halfedges
              << " | Inconsistent duplicate halfedges: " << duplicated_halfedges << std::endl;

    // 2. Exact Self-Intersection Pairs Inspection
    std::vector<std::pair<ExactMesh::Face_index, ExactMesh::Face_index>> intersected_pairs;
    CGAL::Polygon_mesh_processing::self_intersections(solid_wedge, std::back_inserter(intersected_pairs));
    if (!intersected_pairs.empty()) {
        std::cout << "    [Self-Intersections] Total intersecting face pairs: " << intersected_pairs.size() << std::endl;
        for (size_t i = 0; i < (std::min)(intersected_pairs.size(), size_t(5)); ++i) {
            auto f1 = intersected_pairs[i].first;
            auto f2 = intersected_pairs[i].second;
            std::cout << "      Pair #" << (i + 1) << ": Face " << f1 << " vs Face " << f2 << std::endl;
        }
    }

    FT total_area = CGAL::Polygon_mesh_processing::area(solid_wedge);

    return {solid_wedge, source_faces, total_area};
}

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

    ExactMesh dummy_mesh = mesh_part;
    FaceBoolMap dummy_handled = dummy_mesh.add_property_map<ExactMesh::Face_index, bool>("f:dummy", false).first;

    auto v1 = compute_exact_upper_envelope_mesh(mesh_part, face_descriptors, face_normals, dummy_handled, d1, FT(0));
    auto v2 = compute_exact_upper_envelope_mesh(mesh_part, face_descriptors, face_normals, dummy_handled, d2, FT(0));

    std::set<size_t> covered = v1.source_faces;
    covered.insert(v2.source_faces.begin(), v2.source_faces.end());

    std::vector<int> trapped_faces;
    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
        if (!covered.count(f_idx)) {
            trapped_faces.push_back((int)f_idx);
        }
    }

    if (trapped_faces.empty()) return {};

    std::map<int, int> face_to_trapped_idx;
    for (size_t i = 0; i < trapped_faces.size(); ++i) {
        face_to_trapped_idx[trapped_faces[i]] = (int)i;
    }

    DSU dsu((int)trapped_faces.size());
    for (const auto& [edge, faces] : edge_to_faces) {
        std::vector<int> trapped_in_edge;
        for (int f_idx : faces) {
            auto it = face_to_trapped_idx.find(f_idx);
            if (it != face_to_trapped_idx.end()) {
                trapped_in_edge.push_back(it->second);
            }
        }
        if (trapped_in_edge.size() >= 2) {
            for (size_t i = 1; i < trapped_in_edge.size(); ++i) {
                dsu.unite(trapped_in_edge[0], trapped_in_edge[i]);
            }
        }
    }

    std::map<int, std::vector<int>> clusters_map;
    for (size_t i = 0; i < trapped_faces.size(); ++i) {
        int root = dsu.find((int)i);
        clusters_map[root].push_back(trapped_faces[i]);
    }

    std::vector<UndercutCluster> clusters;
    for (const auto& [root, f_indices] : clusters_map) {
        EK::Vector_3 avg_n(FT(0), FT(0), FT(0));
        for (int f_idx : f_indices) {
            avg_n = avg_n + face_normals[f_idx];
        }
        double an_x = CGAL::to_double(avg_n.x());
        double an_y = CGAL::to_double(avg_n.y());
        double an_z = CGAL::to_double(avg_n.z());
        double len = std::sqrt(an_x*an_x + an_y*an_y + an_z*an_z);
        if (len > 1e-9) {
            avg_n = EK::Vector_3(FT(an_x / len), FT(an_y / len), FT(an_z / len));
        } else {
            avg_n = EK::Vector_3(FT(0), FT(0), FT(1));
        }

        clusters.push_back({f_indices, avg_n, (int)f_indices.size()});
    }

    return clusters;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
