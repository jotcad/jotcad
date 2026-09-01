#pragma once
#include "types.h"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/Env_triangle_traits_3.h>
#include <CGAL/Env_surface_data_traits_3.h>
#include <CGAL/envelope_3.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/rational_rotation.h>
#include <list>

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
    int nesting_level() const { return _info._nesting_level; }
    void set_nesting_level(int l) { _info._nesting_level = l; }
};

typedef CGAL::Triangulation_vertex_base_2<EK> CDTVb;
typedef CGAL::Constrained_triangulation_face_base_2<EK> CDTFb;
typedef CDT_Face_with_info<EK, CDTFb> CDTFb_with_info;
typedef CGAL::Triangulation_data_structure_2<CDTVb, CDTFb_with_info> CDTTDS;
typedef CGAL::Constrained_Delaunay_triangulation_2<EK, CDTTDS, CGAL::Exact_intersections_tag> ExactCDT;

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
    // 100% Exact Rational Rotation via CGAL::rational_rotation_approximation (Zero floats downstream)
    double dx_d = CGAL::to_double(d.x());
    double dy_d = CGAL::to_double(d.y());
    double dz_d = CGAL::to_double(d.z());

    double phi = std::atan2(dy_d, dx_d);
    double theta = std::atan2(std::sqrt(dx_d * dx_d + dy_d * dy_d), dz_d);

    EK::RT sin_phi, cos_phi, w_phi;
    CGAL::rational_rotation_approximation(-phi, sin_phi, cos_phi, w_phi, EK::RT(1), EK::RT(1000000));
    CGAL::Aff_transformation_3<EK> Rz(
        cos_phi, -sin_phi, 0, 0,
        sin_phi, cos_phi, 0, 0,
        0, 0, w_phi, 0,
        w_phi
    );

    EK::RT sin_theta, cos_theta, w_theta;
    CGAL::rational_rotation_approximation(-theta, sin_theta, cos_theta, w_theta, EK::RT(1), EK::RT(1000000));
    CGAL::Aff_transformation_3<EK> Ry(
        cos_theta, 0, sin_theta, 0,
        0, w_theta, 0, 0,
        -sin_theta, 0, cos_theta, 0,
        w_theta
    );

    CGAL::Aff_transformation_3<EK> to_z = Ry * Rz;
    CGAL::Aff_transformation_3<EK> from_z = to_z.inverse();

    FT p_xmin, p_xmax, p_ymin, p_ymax, p_zmin;
    bool has_seed = !seed_patch_faces.empty();
    if (has_seed) {
        bool first = true;
        for (auto f : seed_patch_faces) {
            auto h = mesh_part.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                auto p_rot = to_z(mesh_part.point(mesh_part.source(h)));
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
        auto p0 = to_z(mesh_part.point(mesh_part.source(h)));
        auto p1 = to_z(mesh_part.point(mesh_part.target(h)));
        auto p2 = to_z(mesh_part.point(mesh_part.target(mesh_part.next(h))));

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
            EK::Point_3 ceil_p2(p2_2d.x(), p2_2d.y(), h_ceiling_rot);

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

    // 2. Helper to add a clean 2D-triangulated vertical wall between height lists along directed halfedge h
    auto add_vertical_wall = [&](Envelope_diagram_2::Halfedge_handle h, FT low_s, FT low_t, FT high_s, FT high_t) {
        if (low_s == high_s && low_t == high_t) return;
        auto p1_2d = h->source()->point();
        auto p2_2d = h->target()->point();

        std::vector<FT> left_zs;
        for (FT z : vertex_heights[h->source()]) {
            if (z >= low_s && z <= high_s) left_zs.push_back(z);
        }
        std::sort(left_zs.begin(), left_zs.end());
        if (left_zs.empty() || left_zs.front() > low_s) left_zs.insert(left_zs.begin(), low_s);
        if (left_zs.back() < high_s) left_zs.push_back(high_s);

        std::vector<FT> right_zs;
        for (FT z : vertex_heights[h->target()]) {
            if (z >= low_t && z <= high_t) right_zs.push_back(z);
        }
        std::sort(right_zs.begin(), right_zs.end());
        if (right_zs.empty() || right_zs.front() > low_t) right_zs.insert(right_zs.begin(), low_t);
        if (right_zs.back() < high_t) right_zs.push_back(high_t);

        // Monotonic zip triangulation between left_zs and right_zs
        // Winding order for outward normal to the RIGHT of p1 -> p2:
        // Left advance:  (p2, right_zs[j]) -> (p1, left_zs[i]) -> (p1, left_zs[i+1])
        // Right advance: (p2, right_zs[j]) -> (p1, left_zs[i]) -> (p2, right_zs[j+1])
        size_t i = 0, j = 0;
        size_t N = left_zs.size(), M = right_zs.size();
        FT span_L = high_s - low_s;
        FT span_R = high_t - low_t;

        while (i + 1 < N || j + 1 < M) {
            bool advance_left = false;
            if (i + 1 < N && j + 1 < M) {
                FT t_L = (span_L > FT(0)) ? (left_zs[i + 1] - low_s) / span_L : FT(1);
                FT t_R = (span_R > FT(0)) ? (right_zs[j + 1] - low_t) / span_R : FT(1);
                advance_left = (t_L <= t_R);
            } else if (i + 1 < N) {
                advance_left = true;
            } else {
                advance_left = false;
            }

            if (advance_left) {
                size_t idx = soup_points.size();
                soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j]));
                soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i]));
                soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i + 1]));
                soup_polygons.push_back({idx, idx + 1, idx + 2});
                i++;
            } else {
                size_t idx = soup_points.size();
                soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j]));
                soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i]));
                soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j + 1]));
                soup_polygons.push_back({idx, idx + 1, idx + 2});
                j++;
            }
        }
    };

    // 3. Process all directed halfedges for both internal step cliffs and outer sidewalls
    auto process_halfedge_walls = [&](Envelope_diagram_2::Halfedge_handle h, size_t orig_f) {
        auto p1_2d = h->source()->point();
        auto p2_2d = h->target()->point();

        FT z1_s = get_z(orig_f, p1_2d.x(), p1_2d.y());
        FT z1_t = get_z(orig_f, p2_2d.x(), p2_2d.y());

        auto twin_face = h->twin()->face();
        if (twin_face->is_unbounded() || twin_face->number_of_surfaces() == 0) {
            // Outer sidewall boundary: sweep from surface height up to ceiling
            add_vertical_wall(h, z1_s, z1_t, h_ceiling_rot, h_ceiling_rot);
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
                        add_vertical_wall(h, z2_s, z2_t, z1_s, z1_t);
                    } else if (sum2 > sum1) {
                        // Face 2 is higher: use twin (p2 -> p1), drop from Face 2 down to Face 1
                        add_vertical_wall(h->twin(), z1_t, z1_s, z2_t, z2_s);
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
                std::cout << "      [Open Gap #" << single_halfedges << "] 2D: ("
                          << CGAL::to_double(p0.x()) << ", " << CGAL::to_double(p0.y()) << ") -> ("
                          << CGAL::to_double(p1.x()) << ", " << CGAL::to_double(p1.y()) << ")"
                          << " | Z: " << CGAL::to_double(p0.z()) << " -> " << CGAL::to_double(p1.z())
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
        for (size_t i = 0; i < (std::min)(intersected_pairs.size(), size_t(3)); ++i) {
            auto f1 = intersected_pairs[i].first;
            auto f2 = intersected_pairs[i].second;
            std::cout << "      Pair #" << (i + 1) << ": Face " << f1 << " vs Face " << f2 << std::endl;
            auto print_f = [&](ExactMesh::Face_index f, const char* name) {
                std::cout << "        " << name << " " << f << ": ";
                auto h = solid_wedge.halfedge(f);
                auto curr = h;
                do {
                    auto v = solid_wedge.target(curr);
                    auto p = solid_wedge.point(v);
                    auto pr = to_z(p);
                    std::cout << "v" << v.idx() << "(" << pr.x() << ", " << pr.y() << ", " << pr.z() << ") ";
                    curr = solid_wedge.next(curr);
                } while (curr != h);
                std::cout << std::endl;
            };
            print_f(f1, "f1");
            print_f(f2, "f2");
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
