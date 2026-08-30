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
    std::map<int, ExactMesh::Vertex_index> v_bot_map;
    std::map<int, ExactMesh::Vertex_index> v_top_map;

    FT h_extrude = FT(100);

    auto get_bot_v = [&](int orig_v_idx) -> ExactMesh::Vertex_index {
        auto it = v_bot_map.find(orig_v_idx);
        if (it != v_bot_map.end()) return it->second;
        auto p = mesh_part.point(ExactMesh::Vertex_index(orig_v_idx));
        auto v = solid_wedge.add_vertex(p);
        v_bot_map[orig_v_idx] = v;
        return v;
    };

    auto get_top_v = [&](int orig_v_idx) -> ExactMesh::Vertex_index {
        auto it = v_top_map.find(orig_v_idx);
        if (it != v_top_map.end()) return it->second;
        auto p = mesh_part.point(ExactMesh::Vertex_index(orig_v_idx));
        EK::Point_3 top_p(p.x() + d.x() * h_extrude, p.y() + d.y() * h_extrude, p.z() + d.z() * h_extrude);
        auto v = solid_wedge.add_vertex(top_p);
        v_top_map[orig_v_idx] = v;
        return v;
    };

    const auto& patch_faces = seed_patch_faces.empty() ? face_descriptors : seed_patch_faces;
    std::set<size_t> source_faces;

    // 1. Reversed Model Cavity Faces directly in World Space
    for (auto f : patch_faces) {
        if (is_handled[f]) continue;
        source_faces.insert((size_t)f);

        auto h = mesh_part.halfedge(f);
        int i0 = (int)mesh_part.source(h);
        int i1 = (int)mesh_part.target(h);
        int i2 = (int)mesh_part.target(mesh_part.next(h));

        auto v0 = get_bot_v(i0);
        auto v1 = get_bot_v(i1);
        auto v2 = get_bot_v(i2);
        solid_wedge.add_face(v0, v2, v1);
    }

    // 2. Extruded Sidewalls on Border Halfedges directly in World Space
    std::vector<ExactMesh::Halfedge_index> border_halfedges;
    CGAL::Polygon_mesh_processing::border_halfedges(patch_faces, mesh_part, std::back_inserter(border_halfedges));
    for (auto h : border_halfedges) {
        int u_idx = (int)mesh_part.source(h);
        int v_idx = (int)mesh_part.target(h);

        auto u_bot = get_bot_v(u_idx);
        auto v_bot = get_bot_v(v_idx);
        auto u_top = get_top_v(u_idx);
        auto v_top = get_top_v(v_idx);

        solid_wedge.add_face(u_bot, v_bot, v_top);
        solid_wedge.add_face(u_bot, v_top, u_top);
    }

    // 3. Top Planar Ceiling Cap
    EK::Point_3 top_center(0, 0, 0);
    FT count = 0;
    for (auto h : border_halfedges) {
        int u_idx = (int)mesh_part.source(h);
        auto p = mesh_part.point(ExactMesh::Vertex_index(u_idx));
        top_center = EK::Point_3(top_center.x() + p.x() + d.x() * h_extrude,
                                 top_center.y() + p.y() + d.y() * h_extrude,
                                 top_center.z() + p.z() + d.z() * h_extrude);
        count = count + FT(1);
    }
    if (count > FT(0)) {
        top_center = EK::Point_3(top_center.x() / count, top_center.y() / count, top_center.z() / count);
        auto c_top = solid_wedge.add_vertex(top_center);
        for (auto h : border_halfedges) {
            int u_idx = (int)mesh_part.source(h);
            int v_idx = (int)mesh_part.target(h);
            auto u_top = get_top_v(u_idx);
            auto v_top = get_top_v(v_idx);
            solid_wedge.add_face(v_top, u_top, c_top);
        }
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
