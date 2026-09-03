#pragma once
#include "types.h"
#include "rotation.h"
#include "walls.h"
#include "diagnostics.h"
#include "fix/kiss.h"
#include "fix/assert_mesh.h"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/IO/polygon_mesh_io.h>
#include <filesystem>
#include <chrono>
#include <list>
#include <queue>

namespace jotcad {
namespace geo {
namespace mold {

struct CDTFaceInfo {
    bool in_domain = false;
    int _nesting_level = 0;
};

template <typename Gt, typename Fb_base = CGAL::Constrained_triangulation_face_base_2<Gt>>
class CDT_Face_with_info : public Fb_base {
    CDTFaceInfo _info;
public:
    typedef Gt Geom_traits;
    typedef typename Fb_base::Vertex_handle Vertex_handle;
    typedef typename Fb_base::Face_handle   Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Fb_base::template Rebind_TDS<TDS2>::Other Fb2;
        typedef CDT_Face_with_info<Gt, Fb2> Other;
    };

    CDT_Face_with_info() : Fb_base() {}
    CDT_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2)
        : Fb_base(v0, v1, v2) {}
    CDT_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2,
                       Face_handle n0, Face_handle n1, Face_handle n2)
        : Fb_base(v0, v1, v2, n0, n1, n2) {}
    CDTFaceInfo& info() { return _info; }
    const CDTFaceInfo& info() const { return _info; }
    bool is_in_domain() const { return _info.in_domain; }
    void set_in_domain(bool b) { _info.in_domain = b; }
};

typedef CGAL::Exact_predicates_exact_constructions_kernel CDT_Kernel;
typedef CGAL::Triangulation_vertex_base_2<CDT_Kernel> CDT_Vb;
typedef CDT_Face_with_info<CDT_Kernel> CDT_Fb;
typedef CGAL::Triangulation_data_structure_2<CDT_Vb, CDT_Fb> CDT_TDS;
typedef CGAL::Exact_intersections_tag CDT_Itag;
typedef CGAL::Constrained_Delaunay_triangulation_2<CDT_Kernel, CDT_TDS, CDT_Itag> ExactCDT;

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
    auto [to_z, from_z] = compute_exact_z_rotation(d);

    std::set<ExactMesh::Face_index> candidate_faces;
    bool has_seed = !seed_patch_faces.empty();
    if (has_seed) {
        std::queue<ExactMesh::Face_index> q;
        for (auto f : seed_patch_faces) {
            candidate_faces.insert(f);
            q.push(f);
        }
        while (!q.empty()) {
            auto curr_f = q.front();
            q.pop();
            auto h = mesh_part.halfedge(curr_f);
            auto h_start = h;
            do {
                auto h_twin = mesh_part.opposite(h);
                if (h_twin != ExactMesh::null_halfedge()) {
                    auto neighbor_f = mesh_part.face(h_twin);
                    if (neighbor_f != ExactMesh::null_face() && !is_handled[neighbor_f]) {
                        size_t n_idx = neighbor_f.idx();
                        if (face_normals[n_idx] * d >= min_dot) {
                            if (candidate_faces.insert(neighbor_f).second) {
                                q.push(neighbor_f);
                            }
                        }
                    }
                }
                h = mesh_part.next(h);
            } while (h != h_start);
        }
    } else {
        std::set<ExactMesh::Face_index> eligible;
        for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
            auto f = face_descriptors[f_idx];
            if (!is_handled[f] && face_normals[f_idx] * d >= min_dot) {
                eligible.insert(f);
            }
        }
        std::set<ExactMesh::Face_index> visited;
        std::vector<std::vector<ExactMesh::Face_index>> components;
        for (auto f : eligible) {
            if (visited.count(f)) continue;
            std::vector<ExactMesh::Face_index> comp;
            std::queue<ExactMesh::Face_index> q;
            q.push(f);
            visited.insert(f);
            while (!q.empty()) {
                auto curr = q.front();
                q.pop();
                comp.push_back(curr);
                auto h = mesh_part.halfedge(curr);
                auto h_start = h;
                do {
                    auto h_twin = mesh_part.opposite(h);
                    if (h_twin != ExactMesh::null_halfedge()) {
                        auto n_f = mesh_part.face(h_twin);
                        if (n_f != ExactMesh::null_face() && eligible.count(n_f)) {
                            if (visited.insert(n_f).second) {
                                q.push(n_f);
                            }
                        }
                    }
                    h = mesh_part.next(h);
                } while (h != h_start);
            }
            components.push_back(comp);
        }
        size_t max_sz = 0;
        for (const auto& comp : components) {
            if (comp.size() > max_sz) {
                max_sz = comp.size();
                candidate_faces = std::set<ExactMesh::Face_index>(comp.begin(), comp.end());
            }
        }
    }

    std::map<size_t, std::array<EK::Point_3, 3>> rotated_tris;
    std::list<Data_triangle_3> triangles;
    for (auto f : candidate_faces) {
        size_t f_idx = f.idx();
        auto h = mesh_part.halfedge(f);
        auto p0 = to_z(mesh_part.point(mesh_part.source(h)));
        auto p1 = to_z(mesh_part.point(mesh_part.target(h)));
        auto p2 = to_z(mesh_part.point(mesh_part.target(mesh_part.next(h))));

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

    std::vector<EK::Point_3> soup_points;
    std::vector<std::vector<size_t>> soup_polygons;
    std::set<size_t> source_faces;

    // 1. Add all illuminated surface cells (floor + symmetrical ceiling) via uniform 2D CDT
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;

        size_t orig_f_idx = fit->surfaces_begin()->data();
        source_faces.insert(orig_f_idx);

        ExactCDT cdt;
        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        std::vector<ExactCDT::Vertex_handle> outer_vh;
        do {
            auto p2d = curr->target()->point();
            outer_vh.push_back(cdt.insert(p2d));
            curr = curr->next();
        } while (curr != ccb);

        if (outer_vh.size() >= 3) {
            for (size_t i = 0; i < outer_vh.size(); ++i) {
                cdt.insert_constraint(outer_vh[i], outer_vh[(i + 1) % outer_vh.size()]);
            }
        }

        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            std::vector<ExactCDT::Vertex_handle> hole_vh;
            do {
                auto p2d = h_curr->target()->point();
                hole_vh.push_back(cdt.insert(p2d));
                h_curr = h_curr->next();
            } while (h_curr != h_start);

            if (hole_vh.size() >= 3) {
                for (size_t i = 0; i < hole_vh.size(); ++i) {
                    cdt.insert_constraint(hole_vh[i], hole_vh[(i + 1) % hole_vh.size()]);
                }
            }
        }

        CGAL::mark_domain_in_triangulation(cdt);

        for (auto cdt_fit = cdt.finite_faces_begin(); cdt_fit != cdt.finite_faces_end(); ++cdt_fit) {
            if (!cdt_fit->info().in_domain) continue;

            auto p0_2d = cdt_fit->vertex(0)->point();
            auto p1_2d = cdt_fit->vertex(1)->point();
            auto p2_2d = cdt_fit->vertex(2)->point();

            FT vz0 = get_z(orig_f_idx, p0_2d.x(), p0_2d.y());
            FT vz1 = get_z(orig_f_idx, p1_2d.x(), p1_2d.y());
            FT vz2 = get_z(orig_f_idx, p2_2d.x(), p2_2d.y());

            EK::Point_3 floor_p0(p0_2d.x(), p0_2d.y(), vz0);
            EK::Point_3 floor_p1(p1_2d.x(), p1_2d.y(), vz1);
            EK::Point_3 floor_p2(p2_2d.x(), p2_2d.y(), vz2);

            EK::Point_3 ceil_p0(p0_2d.x(), p0_2d.y(), h_ceiling_rot);
            EK::Point_3 ceil_p1(p1_2d.x(), p1_2d.y(), h_ceiling_rot);
            EK::Point_3 ceil_p2(floor_p2.x(), floor_p2.y(), h_ceiling_rot);

            // Floor triangle (matching CDT CCW winding -> CW in 3D for downward -Z outward normal)
            size_t idx0 = soup_points.size();
            soup_points.push_back(floor_p0);
            soup_points.push_back(floor_p2);
            soup_points.push_back(floor_p1);
            soup_polygons.push_back({idx0, idx0 + 1, idx0 + 2});

            // Ceiling triangle (CCW winding for upward +Z outward normal)
            size_t c_idx0 = soup_points.size();
            soup_points.push_back(ceil_p0);
            soup_points.push_back(ceil_p1);
            soup_points.push_back(ceil_p2);
            soup_polygons.push_back({c_idx0, c_idx0 + 1, c_idx0 + 2});
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

    // 2. Process all directed halfedges for both internal step cliffs and outer sidewalls
    auto process_halfedge_walls = [&](Envelope_diagram_2::Halfedge_handle h, size_t orig_f) {
        auto p1_2d = h->source()->point();
        auto p2_2d = h->target()->point();

        FT z1_s = get_z(orig_f, p1_2d.x(), p1_2d.y());
        FT z1_t = get_z(orig_f, p2_2d.x(), p2_2d.y());

        auto twin_face = h->twin()->face();
        if (twin_face->is_unbounded() || twin_face->number_of_surfaces() == 0) {
            // Outer sidewall boundary: sweep from surface height up to ceiling
            add_monotonic_vertical_wall(h, z1_s, z1_t, h_ceiling_rot, h_ceiling_rot, vertex_heights, soup_points, soup_polygons);
        } else {
            // Internal boundary: process each undirected edge exactly once using pointer ordering
            if (h < h->twin()) {
                size_t orig_f2 = twin_face->surfaces_begin()->data();
                if (orig_f != orig_f2) {
                    FT z2_s = get_z(orig_f2, p1_2d.x(), p1_2d.y());
                    FT z2_t = get_z(orig_f2, p2_2d.x(), p2_2d.y());

                    FT sum1 = z1_s + z1_t;
                    FT sum2 = z2_s + z2_t;

                    if (sum1 > sum2) {
                        // Face 1 is higher: use h (p1 -> p2), drop from Face 1 down to Face 2
                        add_monotonic_vertical_wall(h, z2_s, z2_t, z1_s, z1_t, vertex_heights, soup_points, soup_polygons);
                    } else if (sum2 > sum1) {
                        // Face 2 is higher: use twin (p2 -> p1), drop from Face 2 down to Face 1
                        add_monotonic_vertical_wall(h->twin(), z1_t, z1_s, z2_t, z2_s, vertex_heights, soup_points, soup_polygons);
                    }
                }
            }
        }
    };

    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;
        size_t orig_f = fit->surfaces_begin()->data();

        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do {
            process_halfedge_walls(curr, orig_f);
            curr = curr->next();
        } while (curr != ccb);

        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            do {
                process_halfedge_walls(h_curr, orig_f);
                h_curr = h_curr->next();
            } while (h_curr != h_start);
        }
    }

    if (soup_polygons.empty()) return {};

    // Diagnostic audits
    audit_vertex_umbrella(soup_points, soup_polygons, EK::Point_3(FT(-11882) / FT(1000), FT(-10501) / FT(1000), FT(-8174) / FT(1000)));

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
    fix::assert_well_formed_for_corefinement(solid_wedge, "raw solid_wedge from polygon soup in envelope.h");

    if (CGAL::is_closed(solid_wedge)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(solid_wedge);
    }

    // Transform solid_wedge back from +Z frame to world space in one exact affine operation
    for (auto v : solid_wedge.vertices()) {
        solid_wedge.point(v) = from_z(solid_wedge.point(v));
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

    if (self_intersects) {
        std::filesystem::create_directories("scratch");
        CGAL::IO::write_polygon_mesh("scratch/self_touch_wedge.off", solid_wedge);
        std::cout << "    [Disambiguation] Resolving zero-volume touches with separate_kissing_columns..." << std::endl << std::flush;
        fix::separate_kissing_columns(solid_wedge, pinch_bridge_width_ft());
        CGAL::Polygon_mesh_processing::triangulate_faces(solid_wedge);
        self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(solid_wedge);
        std::cout << "    [Disambiguation Result] does_self_intersect: " << (self_intersects ? "YES" : "NO") << std::endl << std::flush;
    }

    audit_polygon_soup(soup_points, soup_polygons);
    inspect_self_intersections(solid_wedge, to_z);

    FT total_area = CGAL::Polygon_mesh_processing::area(solid_wedge);
    fix::assert_well_formed_mesh(solid_wedge, "solid_wedge in compute_exact_upper_envelope_mesh");
    return {solid_wedge, source_faces, total_area};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
