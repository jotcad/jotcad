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

    ExactMesh solid_wedge;
    std::map<std::tuple<FT, FT, FT>, ExactMesh::Vertex_index> pt_map;

    auto get_v = [&](FT rx, FT ry, FT rz) -> ExactMesh::Vertex_index {
        auto key = std::make_tuple(rx, ry, rz);
        auto it = pt_map.find(key);
        if (it != pt_map.end()) return it->second;
        EK::Point_3 world_p = unrotate_pt(rx, ry, rz);
        auto v = solid_wedge.add_vertex(world_p);
        pt_map[key] = v;
        return v;
    };

    std::set<size_t> source_faces;
    FT max_vz_rot = -1000000;

    // 1. Add all illuminated surface cells (triangulated)
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;

        size_t orig_f_idx = fit->surfaces_begin()->data();
        source_faces.insert(orig_f_idx);

        std::vector<Envelope_diagram_2::Point_2> pts_2d;
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            auto p2d = curr->target()->point();
            pts_2d.push_back(p2d);
            FT vz = get_z(orig_f_idx, p2d.x(), p2d.y());
            if (vz > max_vz_rot) max_vz_rot = vz;
            curr = curr->next();
        } while (curr != ccb);

        if (pts_2d.size() >= 3) {
            auto v0 = get_v(pts_2d[0].x(), pts_2d[0].y(), get_z(orig_f_idx, pts_2d[0].x(), pts_2d[0].y()));
            for (size_t i = 1; i + 1 < pts_2d.size(); ++i) {
                auto v1 = get_v(pts_2d[i].x(), pts_2d[i].y(), get_z(orig_f_idx, pts_2d[i].x(), pts_2d[i].y()));
                auto v2 = get_v(pts_2d[i + 1].x(), pts_2d[i + 1].y(), get_z(orig_f_idx, pts_2d[i + 1].x(), pts_2d[i + 1].y()));
                solid_wedge.add_face(v0, v1, v2);
            }
        }
    }

    auto add_cliff = [&](ExactMesh::Vertex_index u_h, ExactMesh::Vertex_index v_h,
                         ExactMesh::Vertex_index v_l, ExactMesh::Vertex_index u_l) {
        if (u_h == u_l && v_h == v_l) return;
        if (u_h == u_l) {
            solid_wedge.add_face(v_h, u_h, v_l);
        } else if (v_h == v_l) {
            solid_wedge.add_face(v_h, u_h, u_l);
        } else {
            solid_wedge.add_face(v_h, u_h, u_l);
            solid_wedge.add_face(v_h, u_l, v_l);
        }
    };

    // 2. Add vertical cliff quads at step discontinuities
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
            auto u1 = get_v(p1_2d.x(), p1_2d.y(), z1_s);
            auto v1 = get_v(p2_2d.x(), p2_2d.y(), z1_t);
            auto v2 = get_v(p2_2d.x(), p2_2d.y(), z2_t);
            auto u2 = get_v(p1_2d.x(), p1_2d.y(), z2_s);

            if ((z1_s + z1_t) >= (z2_s + z2_t)) {
                add_cliff(u1, v1, v2, u2);
            } else {
                add_cliff(v2, u2, u1, v1);
            }
        }
    }

    // 3. Add swept sidewalls for true outer/aperture boundary halfedges
    FT h_ceiling_rot = max_vz_rot + FT(50);

    auto add_sidewall = [&](Envelope_diagram_2::Halfedge_handle h, size_t orig_f) {
        if (h->twin()->face()->is_unbounded() || h->twin()->face()->number_of_surfaces() == 0) {
            auto p1_2d = h->source()->point();
            auto p2_2d = h->target()->point();

            FT z_s = get_z(orig_f, p1_2d.x(), p1_2d.y());
            FT z_t = get_z(orig_f, p2_2d.x(), p2_2d.y());

            auto u_bot = get_v(p1_2d.x(), p1_2d.y(), z_s);
            auto v_bot = get_v(p2_2d.x(), p2_2d.y(), z_t);
            auto v_top = get_v(p2_2d.x(), p2_2d.y(), h_ceiling_rot);
            auto u_top = get_v(p1_2d.x(), p1_2d.y(), h_ceiling_rot);

            if (u_bot == u_top) {
                solid_wedge.add_face(v_bot, u_bot, v_top);
            } else if (v_bot == v_top) {
                solid_wedge.add_face(v_bot, u_bot, u_top);
            } else {
                solid_wedge.add_face(v_bot, u_bot, u_top);
                solid_wedge.add_face(v_bot, u_top, v_top);
            }
        }
    };

    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f = fit->surfaces_begin()->data();

        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            add_sidewall(curr, orig_f);
            curr = curr->next();
        } while (curr != ccb);

        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            do {
                add_sidewall(h_curr, orig_f);
                h_curr = h_curr->next();
            } while (h_curr != h_start);
        }
    }

    // 4. Seal top ceiling holes
    auto assert_is_simple_border = [&](ExactMesh::Halfedge_index h_start) -> bool {
        auto h = h_start;
        std::vector<EK::Point_2> pts_2d;
        std::set<ExactMesh::Vertex_index> seen_v;
        bool has_duplicate = false;
        int n = 0;
        do {
            auto v = solid_wedge.target(h);
            if (seen_v.count(v)) has_duplicate = true;
            seen_v.insert(v);
            auto p_rot = rotate_pt(solid_wedge.point(v));
            pts_2d.push_back(EK::Point_2(p_rot.x(), p_rot.y()));
            n++;
            h = solid_wedge.next(h);
        } while (h != h_start && n < 20000);
        CGAL::Polygon_2<EK> poly(pts_2d.begin(), pts_2d.end());
        bool is_simple = poly.is_simple();
        std::cout << "  [Border Assertion] Cycle vertices: " << n
                  << " | unique: " << seen_v.size()
                  << " | has_duplicate: " << (has_duplicate ? "YES" : "NO")
                  << " | is_simple: " << (is_simple ? "YES" : "NO") << std::endl << std::flush;
        return is_simple;
    };

    std::vector<ExactMesh::Halfedge_index> border_halfedges;
    CGAL::Polygon_mesh_processing::extract_boundary_cycles(solid_wedge, std::back_inserter(border_halfedges));
    for (auto h_border : border_halfedges) {
        assert_is_simple_border(h_border);
        std::vector<ExactMesh::Face_index> patch_facets;
        CGAL::Polygon_mesh_processing::triangulate_hole(solid_wedge, h_border, std::back_inserter(patch_facets));
    }
    CGAL::Polygon_mesh_processing::stitch_borders(solid_wedge);
    solid_wedge.collect_garbage();

    if (CGAL::is_closed(solid_wedge)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(solid_wedge);
    }

    bool is_closed = CGAL::is_closed(solid_wedge);
    bool self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(solid_wedge);
    std::cout << "  [Wedge Validation] is_closed: " << (is_closed ? "YES" : "NO")
              << " | does_self_intersect: " << (self_intersects ? "YES" : "NO")
              << " | vertices: " << solid_wedge.number_of_vertices()
              << " | faces: " << solid_wedge.number_of_faces() << std::endl << std::flush;

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
